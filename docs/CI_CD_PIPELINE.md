# CI/CD Pipeline â€” can-logger-decoder (Day 4)

This document describes the automated CI/CD pipeline architecture, linting
standards, and build matrix configuration for the `can-logger-decoder`
project (v2.0.0).

---

## 1. Pipeline Overview

The pipeline is implemented entirely with GitHub Actions and consists of
three independent workflows:

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `ci_build.yml` | push / PR to `main` / `master` | Compile with multiple GCC/Clang versions and run unit tests |
| `static_analysis.yml` | push / PR to `main` / `master` | Run cppcheck + clang-tidy static analysis |
| `release_artifacts.yml` | push tags matching `v*` | Cross-compile and package release binaries as GitHub Release assets |

All workflows use `concurrency` groups to cancel in-progress runs for the
same branch/PR, preventing stale builds from blocking newer commits.

---

## 2. ci_build.yml â€” Automated Build Pipeline

**Trigger:** `push` and `pull_request` on `main` / `master`

**Build matrix:**

| Compiler | Version | APT package |
|----------|---------|-------------|
| GCC | 13 | `gcc-13` |
| GCC | 12 | `gcc-12` |
| Clang | 16 | `clang-16` |
| Clang | 15 | `clang-15` |

**Steps per matrix job:**

1. Checkout repository (`actions/checkout@v4`)
2. Install compiler + `make` + `cmake`
3. Print compiler version
4. `make clean && make CC=<compiler> all`
5. `make CC=<compiler> test` (all unit test suites)
6. Run `scripts/run_unit_tests.sh`
7. CLI smoke test with `sample.dbc` (`--summary` and `--mock -c 20`)

`fail-fast: false` ensures all matrix combinations run independently even
if one fails.

---

## 3. static_analysis.yml â€” Static Analysis (cppcheck + clang-tidy)

**Trigger:** `push` and `pull_request` on `main` / `master`

### cppcheck job

Runs `cppcheck --enable=all` over `src/`, `include/`, and `tests/` with:
- `--std=c11 --language=c`
- Suppressions for `missingIncludeSystem`, `unusedFunction`, `missingInclude`
- `--error-exitcode=1` so any finding fails the job

### clang-tidy job

1. Configure CMake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` using `clang-16`
2. Run `clang-tidy -p build --warnings-as-errors=*` over all source files
3. Clean up `build/` directory in an `always()` step

**Check set (`.clang-tidy`):** targets memory leaks
(`clang-analyzer-unix.MallocChecker`), uninitialized variables
(`bugprone-*`), null dereference, dead stores, portability, performance,
and readability/stylistic consistency. C++-only checks are disabled since
this is a pure C99 project.

---

## 4. release_artifacts.yml â€” Cross-Compile & Package Release Binaries

**Trigger:** push tags matching `v*`

**Build matrix (cross-compilation):**

| Target triple | Compiler | Artifact suffix |
|---------------|----------|-----------------|
| `x86_64-linux-gnu` | `gcc` | `linux-x86_64` |
| `arm-linux-gnueabihf` | `arm-linux-gnueabihf-gcc` | `linux-armv7hf` |
| `x86_64-w64-mingw32` | `x86_64-w64-mingw32-gcc` | `windows-x86_64` |

**Steps per target:**

1. Checkout repository
2. Extract version from Git tag
3. Install native + cross-compilation toolchains
4. CMake configure + build in `Release` mode
5. Strip binaries (`strip --strip-all`)
6. Package `can_decoder` binary + `LICENSE`, `README.md`, `sample.dbc`, `CHANGELOG.md` into a `.tar.gz`
7. Generate `SHA-256` checksum file
8. Upload both files as GitHub Release assets (`softprops/action-gh-release@v2`)
9. Clean build artifacts (`always()`)

---

## 5. Linting Standards

### `.clang-format` â€” strict C style

Based on LLVM style, customized for C (`Language: C`, `Standard: c11`):

- Indentation: 4 spaces, no tabs (`UseTab: Never`)
- Column limit: 100 characters
- Pointer alignment: Right (`int *ptr`)
- Brace style: Attach (K&R)
- No short blocks/functions/if/loops on a single line
- `SortIncludes: false`, `IncludeBlocks: Preserve`

Enforced locally by `scripts/check_formatting.sh` and on CI via
`clang-format --dry-run --Werror`.

### `.clang-tidy` â€” static analysis checks

Checks enabled (non-exhaustive):

- `clang-analyzer-*` â€” null dereference, dead stores, memory leaks (MallocChecker)
- `bugprone-*` â€” uninitialized variables, resource management, easily-swappable parameters
- `portability-*` â€” portable code constructs
- `performance-*` â€” unnecessary copies, performance anti-patterns
- `readability-*` â€” consistent naming, style, and documentation
- `misc-*` â€” miscellaneous correctness and style

C++-only checks are disabled. `clang-analyzer-cplusplus*` and
`clang-analyzer-optin.cplusplus.VirtualCall` are explicitly excluded.

---

## 6. Local Developer Workflow

Before committing, developers should run the following locally:

```bash
# 1. Build and run unit tests
make clean && make
make test

# 2. Verify formatting conforms to .clang-format
./scripts/check_formatting.sh

# 3. Run static analysis (cppcheck + clang-tidy if available)
./scripts/run_linter.sh

# 4. Run memory safety checks (Valgrind / ASan)
./scripts/run_valgrind.sh
```

`scripts/check_formatting.sh` exits non-zero if any file under `src/`,
`include/`, or `tests/` does not match `.clang-format`.

`scripts/run_linter.sh` runs `cppcheck` and `clang-tidy` when installed,
and gracefully skips missing tools with an informative message.

---

## 7. Hygiene Rules

The following are **never** tracked in the repository (enforced by
`.gitignore`):

- Build artifacts: `*.o`, `build/`, `*.bin`, `bin/`, `target/`
- Logs: `*.log`, `*.csv`, `can_log.csv`, `can_log_*.csv`
- IDE configs: `.vscode/`, `.idea/`, `.antigravity/`, `.cursor/`, `.claude/`
- Compiled binaries: `can_decoder`, `can_decoder.exe`, `test_suite`, `test_suite.exe`

No `git commit` or `git push` is performed as part of Day 4; changes are
reviewed locally first.

---

## 8. Adding a New Compiler Version to the Build Matrix

Edit `.github/workflows/ci_build.yml`, `jobs.build-and-test.strategy.matrix.include`:

```yaml
- compiler: gcc
  compiler-version: '14'
  apt-package: gcc-14
```

Then set `CC`/`CXX` env vars accordingly (already wired via `${{ env.CC }}`).

---

## 9. Workflow File Locations

```
.github/
  workflows/
    ci_build.yml           # CI build + test matrix
    static_analysis.yml    # cppcheck + clang-tidy on PRs
    release_artifacts.yml  # tagged-release cross-compile & packaging
.clang-format              # C formatting standard
.clang-tidy                # static analysis check set
scripts/
  run_linter.sh            # local cppcheck + clang-tidy
  check_formatting.sh      # local clang-format --dry-run
docs/
  CI_CD_PIPELINE.md        # this document
```
