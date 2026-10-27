# Frequently Asked Questions â€” CI/CD Automation (Day 4)

---

### Q1: Why use both `ci_build.yml` and `static_analysis.yml`?
`ci_build.yml` focuses on fast compilation and test execution across multiple
GCC and Clang releases. `static_analysis.yml` runs deeper static analysis
(`cppcheck` and `clang-tidy`) which requires compiling a Clang compilation
database and takes longer to complete.

### Q2: How does the release pipeline cross-compile for ARM?
`.github/workflows/release_artifacts.yml` installs `gcc-arm-linux-gnueabihf`
and configures CMake with `-DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc`. This
produces native ARMv7 binaries suitable for Raspberry Pi without requiring
QEMU emulation.

### Q3: Why are hard tabs forbidden in the repository?
Consistent 4-space indentation ensures uniform rendering across terminal
pagers, web viewers, GitHub diffs, and IDEs. LLVM C style mandates spaces.

### Q4: How do I run static analysis locally before pushing?
Run `./scripts/run_linter.sh`. If `cppcheck` or `clang-tidy` is installed,
the script analyzes your modified code immediately.