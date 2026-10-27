# C Coding Standards & Linter Rule Guide â€” can-logger-decoder (Day 4)

This document establishes the official C coding conventions, static analysis
rules, and linter enforcement standards for the `can-logger-decoder` codebase.

---

## 1. Clang-Format Style Rules (`.clang-format`)

Our style is based on LLVM C with project-specific adaptations:

| Parameter | Value | Description |
|-----------|-------|-------------|
| `Language` | `C` | Pure C standard compliance |
| `Standard` | `c11` | C11 standard feature validation |
| `ColumnLimit` | `100` | Maximum characters per line |
| `IndentWidth` | `4` | 4 spaces per indentation level |
| `TabWidth` | `4` | 4 spaces visual tab representation |
| `UseTab` | `Never` | Hard tabs are strictly prohibited |
| `BreakBeforeBraces` | `Attach` | K&R / LLVM brace placement (`if (...) {`) |
| `PointerAlignment` | `Right` | `uint8_t *buffer`, not `uint8_t* buffer` |
| `ReferenceAlignment`| `Right` | Consistent right alignment |
| `SortIncludes` | `false` | Header inclusion order preserved intentionally |

---

## 2. Clang-Tidy Checks (`.clang-tidy`)

Static analysis via `clang-tidy` is executed with `--warnings-as-errors=*`:

### Enabled Check Categories

- **`bugprone-*`**: Buffer overruns, suspicious sizeof usage, narrowing conversions.
- **`clang-analyzer-*`**: Clang static analyzer memory leaks, null dereferences, dead assignments.
- **`cert-*`**: SEI CERT C safety and secure coding guidelines.
- **`portability-*`**: Cross-platform 32-bit vs 64-bit portability issues.
- **`performance-*`**: Unnecessary memory copies and optimization bottlenecks.
- **`readability-*`**: Identifier naming conventions, uppercase macros, const qualifiers.

### Disabled Categories

- C++ specific checks (`-clang-analyzer-cplusplus*`, `-clang-analyzer-optin.cplusplus.VirtualCall`).
- Non-idiomatic style rules conflicting with Linux/POSIX system programming.

---

## 3. Cppcheck Configuration (`.github/linters/.cppcheck`)

Cppcheck runs on pull requests with `--enable=all --std=c11 --language=c`:

- Strict exit code: `--error-exitcode=1` ensures any finding halts CI.
- System header suppressions to eliminate third-party noise.
- Validates arithmetic overflows, uninitialized pointers, and unread variables.

---

## 4. Source Naming Conventions

- **Functions**: Lowercase with snake_case (`can_parser_init`, `mcp2515_read_register`).
- **Constants & Macros**: UPPERCASE with prefix (`CAN_MAX_DLC`, `CAN_ERR_TIMEOUT`).
- **Types & Typedefs**: Lowercase snake_case ending with `_t` (`can_frame_t`, `can_error_t`).
- **File Names**: Lowercase snake_case (`can_core.h`, `vcan_socket.c`).

---

## 5. Local Enforcement

Before committing any patch:

```bash
# Verify formatting
./scripts/check_formatting.sh

# Fix formatting in-place
./scripts/format_code.sh

# Run local static analysis
./scripts/run_linter.sh
```