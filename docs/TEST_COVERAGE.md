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

## 2. Test Fixtures Catalog (`tests/fixtures/`)

### 2.1 `tests/fixtures/raw_frames.bin`
Binary concatenation of valid, self-delimiting CAN wire frames.
- Format: `[id_b3][id_b2][id_b1][id_b0][dlc][data...]` (4 + 1 + dlc bytes)
- Size: 74 bytes (8 standard, extended, RTR, and error frames)

| # | Type | Raw ID (wire) | DLC | Focus | Matches DBC |
|---|------|---------------|-----|-------|-------------|
| 1 | Standard | `0x00000100` | 8 | Engine_Data | BO_ 256 |
| 2 | Standard | `0x00000101` | 8 | Engine_Status | BO_ 257 |
| 3 | Standard | `0x00000200` | 6 | Brake_System | BO_ 512 |
| 4 | Standard | `0x00000300` | 4 | Transmission_Data | BO_ 768 |
| 5 | Extended | `0x98DAF110` | 8 | 29-bit extended format | Extended Parse |
| 6 | Extended | `0x9FFFFFFF` | 0 | Maximum 29-bit ID | Boundary Check |
| 7 | RTR | `0x40000100` | 0 | Remote request | RTR flag |
| 8 | Error | `0x20000100` | 0 | Error frame | ERR flag |

### 2.2 `tests/fixtures/corrupted_frames.hex`
Text fixture with one hex frame per line and `#` comments. Contains 11 anomalous frames:
- Truncated headers (< 5 bytes) -> `CAN_ERR_IO`
- Out of range DLC values (9, 15) -> `CAN_ERR_INVALID_DLC`
- Payload truncation -> `CAN_ERR_IO`
- High identifier bit normalization -> `CAN_OK`
- Trailing CRC corruption -> checksum validation failure
- Malicious all-0xFF payload -> parsed intact without buffer overflow

### 2.3 `tests/fixtures/extended_frames.bin`
Binary fixture containing 5 extended 29-bit frames (57 bytes), validating SAE J1939 and ISO 14229 UDS frames.

### 2.4 `tests/fixtures/bus_off_sequence.hex`
Escalation hex sequence of 10 error states testing transmitter error counter progression through Warning, Error Passive, and Bus-Off.

---

## 3. Edge-Case Validation Matrix

| Category | Input / Test Method | Expected Behavior | Verification Suite |
|----------|---------------------|-------------------|--------------------|
| **DLC Bounds** | `can_dlc_is_valid(0..8)` | Valid | `test_dlc_mismatch` |
| | `can_dlc_is_valid(9, 15, 255)` | Invalid | `test_dlc_mismatch` |
| | Truncated wire header (< 5 bytes) | `CAN_ERR_IO` | `test_dlc_mismatch` |
| | DLC declared 8, payload 2 bytes | `CAN_ERR_IO` | `test_dlc_mismatch` |
| **Arbitration IDs** | `can_id_clean` on EFF/RTR/ERR | Flag bits stripped cleanly | `test_invalid_arbitration` |
| | Extended ID `0xFFFFFFFF` | Masked to `0x1FFFFFFF` | `test_invalid_arbitration` |
| **Bus-Off Recovery** | TEC > 255 injection | `ERR_STATE_BUS_OFF` | `test_bus_off_trigger` |
| | Recovery controller idle wait | Automatic recovery to NORMAL | `test_bus_off_trigger` |
| **Buffer Overflow** | `EVT_SOCKET_OVERFLOW` | Non-fatal WARNING state | `test_buffer_overflow` |
| | Clean traffic after overflow | Self-healing back to NORMAL | `test_buffer_overflow` |

---

## 4. Running the Tests

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

## 5. Build & Hygiene Specifications

- All build artifacts are emitted into `build/` and ignored by version control.
- Zero private files (`.env`), editor settings (`.vscode/`, `.antigravity/`), or binary artifacts are tracked.
- Compiles cleanly under standard `-Wall -Wextra -std=gnu99 -O2` flags.
