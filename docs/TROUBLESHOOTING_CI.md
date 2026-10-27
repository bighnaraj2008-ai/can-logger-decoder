# Troubleshooting Guide for Failing CI/CD Builds â€” can-logger-decoder (Day 4)

This runbook helps developers diagnose and resolve failures in GitHub Actions
workflows and static analysis runs.

---

## 1. Multi-Compiler Build Matrix Failures (`ci_build.yml`)

### Symptom: Build fails on GCC-12 or Clang-15 but passes locally
- **Cause**: Compiler-specific warnings enabled under `-Wall -Wextra -Werror`
  or subtle differences in C99 standard library headers across distributions.
- **Resolution**:
  1. Inspect the specific compiler step in the GitHub Actions runner log.
  2. Verify code conforms to strict C99 without GNU-specific extensions outside `#ifdef __GNUC__`.
  3. Ensure no uninitialized variables or missing default cases in `switch` statements.

---

## 2. Static Analysis Failures (`static_analysis.yml`)

### Symptom: `clang-tidy` fails with `--warnings-as-errors=*`
- **Cause**: Clang-tidy enforces CERT rules, const correctness, and naming.
- **Resolution**:
  1. Generate `compile_commands.json`:
     ```bash
     cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
     ```
  2. Run local check:
     ```bash
     clang-tidy -p build src/core/can_parser.c
     ```
  3. Apply recommended fixes or cast types cleanly.

### Symptom: `cppcheck` reports false positive
- **Cause**: Dynamic allocation or system call patterns misidentified.
- **Resolution**:
  1. Review `.github/linters/.cppcheck` suppressions list.
  2. Add an inline suppression comment if scoped to a specific line:
     ```c
     // cppcheck-suppress unusedFunction
     ```

---

## 3. Formatting Verification Failures (`check_formatting.sh`)

### Symptom: `check_formatting.sh` exits with code 1
- **Cause**: Inconsistent indent, spaces inside brackets, or line length > 100 characters.
- **Resolution**:
  Run `./scripts/format_code.sh` to auto-format all files according to `.clang-format`.

---

## 4. Cross-Compilation Failures (`release_artifacts.yml`)

### Symptom: MinGW or ARM toolchain linker error
- **Cause**: Missing Linux SocketCAN headers on Windows targets or ABI mismatch.
- **Resolution**:
  The driver isolates Linux-specific headers (`linux/can.h`) behind `#ifdef __linux__`
  and uses mock vehicle interfaces when compiling for Windows. Ensure no direct
  kernel socket includes leak into cross-platform headers (`can_core.h`).