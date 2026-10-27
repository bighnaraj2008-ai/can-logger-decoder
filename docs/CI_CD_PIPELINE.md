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

---

## 5. Linting Standards

### `.clang-format` â€” strict C style

Based on LLVM style, customized for C (`Language: C`, `Standard: c11`):
- Indentation: 4 spaces, no tabs (`UseTab: Never`)
- Column limit: 100 characters
- Pointer alignment: Right (`int *ptr`)
- Brace style: Attach (K&R)

### `.clang-tidy` â€” static analysis checks

Enabled checks target memory safety, undefined behavior, CERT C rules, and
readability guidelines. C++-only checks are excluded.

---

## 6. Local Developer Workflow

```bash
# 1. Build and run unit tests
make clean && make
make test

# 2. Verify formatting conforms to .clang-format
./scripts/check_formatting.sh

# 3. Run static analysis (cppcheck + clang-tidy if available)
./scripts/run_linter.sh
```

---

## 7. Hygiene Rules

The following are **never** tracked in the repository (enforced by `.gitignore`):
- Build artifacts: `*.o`, `build/`, `*.bin`, `bin/`, `target/`
- Logs: `*.log`, `*.csv`
- IDE configs: `.vscode/`, `.idea/`, `.antigravity/`, `.cursor/`, `.claude/`

---

## 9. Workflow File Locations

```
.github/
  workflows/
    ci_build.yml           # CI build + test matrix
    static_analysis.yml    # cppcheck + clang-tidy on PRs
    release_artifacts.yml  # tagged-release cross-compile & packaging
```