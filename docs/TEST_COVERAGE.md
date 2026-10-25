# Test Coverage & Architecture Specifications — Day 2 (`can-logger-decoder` v2.0.0)

This document describes the Day 2 test-fixture and edge-case coverage added to the `can-logger-decoder` repository. It covers test suites, execution harnesses, and validation specifications.

---

## 1. Test Suites Overview

| Suite | File | Focus Area |
|-------|------|------------|
| `test_parser` | `tests/test_parser.c` | DLC bounds, identifier masking, wire round-trip, checksum utilities, acceptance filters, vcan routing |
| `test_state_machine` | `tests/test_state_machine.c` | Error-recovery FSM, fault confinement, bus-off transitions, recovery controller |
| `test_vcan` | `tests/test_vcan.c` | Virtual socket lifecycle, frame I/O |
| `test_filters` | `tests/test_filters.c` | MCP2515 hardware filter/mask register programming |
| `test_metrics` | `tests/test_metrics.c` | Telemetry metrics counters |
| `test_router` | `tests/test_router.c` | Bidirectional frame routing |
| `test_edge_cases` | `tests/edge_cases/*.c` | DLC mismatch, invalid arbitration IDs, bus-off trigger sequences, buffer overflow recovery |
| `test_fixtures` | `tests/test_fixtures.c` | Parser output validated against raw binary and corrupted hex fixtures |

All suites are standalone C99 binaries with a shared `TEST_ASSERT` convention emitting structured `[PASS]` and `[FAIL]` status lines with strict non-zero exit codes on failure.

---

## 2. Running the Tests

### Via Automated Harness (`scripts/run_unit_tests.sh`)
```bash
chmod +x scripts/run_unit_tests.sh
./scripts/run_unit_tests.sh
```

### Via Make
```bash
make all
make test
```

---

## 3. Build & Hygiene Specifications

- All build artifacts are emitted into `build/` and ignored by version control.
- Zero private files (`.env`), editor settings (`.vscode/`, `.antigravity/`), or binary artifacts are tracked.
- Compiles cleanly under standard `-Wall -Wextra -std=gnu99 -O2` flags.
