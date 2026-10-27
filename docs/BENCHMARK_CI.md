# CI Runner Performance Benchmark Tracking Guide â€” can-logger-decoder (Day 4)

This document tracks execution time, memory footprint, and CPU efficiency
across GitHub Actions standard runner environments for CI build jobs.

---

## Benchmark Targets

| Workflow Job | Runner Image | Target Wall-Clock | Peak RAM | Status |
|:-------------|:-------------|:------------------|:---------|:-------|
| `ci_build.yml` (GCC 13) | `ubuntu-latest` | < 45 seconds | < 128 MB | Passing |
| `ci_build.yml` (Clang 16) | `ubuntu-latest` | < 45 seconds | < 160 MB | Passing |
| `static_analysis.yml` (cppcheck) | `ubuntu-latest` | < 30 seconds | < 96 MB | Passing |
| `static_analysis.yml` (clang-tidy) | `ubuntu-latest` | < 60 seconds | < 380 MB | Passing |
| `release_artifacts.yml` | `ubuntu-latest` | < 90 seconds | < 256 MB | Passing |

---

## Runner Tuning Recommendations

1. **Compiler Cache**: Use `ccache` for repetitive runs on self-hosted runners.
2. **Parallel Compilation**: Utilize `-j$(nproc)` in CMake builds.
3. **Selective Analysis**: Only run deep static analysis on changed files in PR workflows.