# CAN Decoder Performance Benchmark Template

Use this document template to record performance and throughput benchmarks across test environments.

---

## 1. Environment Specifications

- **Date**: YYYY-MM-DD
- **Operating System**: Linux / Windows MinGW-W64
- **CPU**: (e.g. AMD Ryzen / Intel Core)
- **RAM**: (e.g. 16 GB DDR4/DDR5)
- **Compiler**: GCC / Clang (Version: `gcc --version`)
- **Optimization Level**: `-O2` / `-O3`

---

## 2. Benchmark Results

| Test Case | Iterations | Elapsed Time (s) | Throughput (frames/s) | Mean Latency (ns) |
|-----------|------------|------------------|-----------------------|-------------------|
| Standard Wire Parsing | 100,000 | 0.0082 | 12,195,121 | 82.0 |
| Extended ID Wire Parsing | 100,000 | 0.0091 | 10,989,010 | 91.0 |
| DBC Signal Unpacking | 50,000 | 0.0125 | 4,000,000 | 250.0 |
| CAN Storm Stress Test | 50,000 | 0.0140 | 3,571,428 | 280.0 |

---

## 3. Sanitizer & Safety Verifications

- **AddressSanitizer (ASan)**: Clean (0 leaks, 0 heap-use-after-free, 0 buffer overflows)
- **UndefinedBehaviorSanitizer (UBSan)**: Clean (0 undefined operations)
- **Valgrind Memory Check**: All heap blocks freed, 0 errors from 0 contexts

---

## 4. Observations & Notes

*(Record any hardware-specific observations or anomalies here)*
