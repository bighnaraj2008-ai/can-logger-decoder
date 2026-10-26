# Contributor Guidelines — C Firmware & Telemetry Decoders

`can-logger-decoder` v2.0.0 · Day 3 Community Standards

Thank you for contributing to `can-logger-decoder`! This document outlines coding standards, development workflows, testing requirements, and submission processes.

---

## 1. Code of Conduct

We are dedicated to providing a friendly, safe, and welcoming environment for all contributors, regardless of experience level, background, or identity.

---

## 2. Development Workflow

1. **Fork & Branch**:
   Create a dedicated feature branch from `main`:
   ```bash
   git checkout -b feat/mcp2515-auto-recovery
   ```
2. **Local Compilation & Verification**:
   Build the library and run all test suites:
   ```bash
   make clean && make
   make test
   ./scripts/run_valgrind.sh
   ```
3. **Commit Atomicity**:
   Keep commits modular and atomic. Stage explicit files only.
4. **Pull Request Submission**:
   Push your branch and submit a Pull Request against `main`. Ensure all GitHub Actions CI checks pass.

---

## 3. C Coding Standards

The project strictly follows the **GNU99 / C99** standard with LLVM styling.

### 3.1 Formatting (.clang-format)
All source and header files must conform to the repository `.clang-format` definition:
- Indentation: 4 spaces (no tabs).
- Column Limit: 100 characters.
- Pointer Alignment: Right (`int *ptr`).
- Braces: Attach to functions and control blocks (K&R style).

Run the formatter prior to committing:
```bash
clang-format -i src/**/*.c include/*.h tests/**/*.c
```

### 3.2 Error Handling Contract
- Public functions MUST return `can_error_t` (defined in `include/error_codes.h`).
- Success is represented by `CAN_OK (0)`.
- Errors are negative integer values.
- Never ignore return values in library code.

---

## 4. Testing Requirements

Every functional addition or bug fix must include corresponding unit tests:
- **Test Coverage**: Maintain 100% pass rate across all 6 test suites (`test_parser`, `test_state_machine`, `test_vcan`, `test_filters`, `test_metrics`, `test_router`).
- **Memory Safety**: Zero memory leaks under AddressSanitizer (`-fsanitize=address`) and Valgrind (`scripts/run_valgrind.sh`).
- **Fixture Integrity**: When modifying test fixtures, run `scripts/validate_fixtures.sh` to update checksum records.

---

## 5. Git Commit Conventions

Commits follow the **Conventional Commits** specification:
- `feat(...)`: New feature or hardware driver addition.
- `fix(...)`: Bug fix or edge-case correction.
- `docs(...)`: Documentation, README, or architecture updates.
- `test(...)`: Unit test additions or mock harness adjustments.
- `chore(...)`: Build script or CI workflow updates.
