# Architectural Refactor — Day 1 (`can-logger-decoder` v2.0.0)

## 1. Summary

Day 1 restructures the monolithic `src/` layout into a layered, modular
architecture while preserving the public API used by `main.c` and the existing
DBC decoder tests. The refactor separates concerns into **core**, **hardware**,
**network**, and **state** layers, introduces explicit error codes, strict DLC
bounds checking, payload checksum utilities, an MCP2515 SPI hardware
abstraction, a virtual SocketCAN (`vcan`) network layer, and an error-recovery
state machine with bus-off recovery.

No git operations are performed in this step; see the repository root for the
verified, un-committed working tree.

## 2. Goals

- **Modularization**: raw CAN frame decoding and payload parsing live in
  dedicated, independently testable source units (`can_frame.c`, `can_parser.c`).
- **Clean headers**: every module has a header with include guards, versioned
  constants, and documented data structures (`can_core.h`, `can_filters.h`,
  `error_codes.h`, plus per-module headers).
- **Defensive parsing**: strict Data Length Code (DLC) bounds checking
  (`can_dlc_is_valid`, `can_dlc_to_bytes`, `can_bytes_to_dlc`) and CRC-8
  payload checksum utilities (`can_checksum_compute/append/validate`).
- **Hardware abstraction**: MCP2515 register map, init sequences and
  acceptance-filter mask configuration are isolated behind a pluggable SPI
  transport (`mcp2515.h`, `mcp2515_spi.c`), so the driver compiles and tests
  cleanly without physical hardware (software register model included).
- **Network abstraction**: virtual CAN (`vcan`) socket binding, non-blocking
  listener polling and frame routing are in `src/network/` (`vcan.h`,
  `vcan.c`), with non-Linux stubs so the tree compiles anywhere.
- **Error recovery**: a robust error-recovery state machine
  (`error_machine.c`, `bus_off_recovery.c`) handles SPI communication faults,
  socket buffer overflows and bus-off conditions with explicit states, events,
  transitions and callbacks.
- **Testability**: every layer is unit-testable on a plain host; no kernel
  drivers or CAN hardware required.
- **Zero-regression**: the existing DBC decoder, CLI, mock bus generator and
  test suite keep working through the preserved `can_parser.h` API.

## 3. Directory layout (after refactor)

```
can-logger-decoder/
├── Makefile                # Layered build, flat objects in build/
├── LICENSE
├── README.md               # Updated to reflect the new layout
├── sample.dbc
├── docs/
│   ├── ARCHITECTURE_REFACTOR.md   (this document)
│   └── STATE_MACHINE.md           # Error recovery FSM reference
├── include/
│   ├── can_core.h          # Version, CAN id masks, DLC limits, inline helpers
│   ├── can_frame.h         # can_frame_t / can_msg_t, wire format, CRC-8 checksum
│   ├── can_filters.h       # Acceptance filter sets, matching, routing
│   ├── error_codes.h       # can_error_t enum + can_error_string()
│   ├── can_parser.h        # Bus handle API (SocketCAN + mock fallback)
│   ├── mcp2515.h           # MCP2515 SPI register map, commands, modes, timing
│   ├── vcan.h              # Virtual CAN socket API + frame routing
│   ├── error_machine.h     # Error-recovery FSM states/events/transitions
│   ├── bus_off_recovery.h  # ISO 11898-1 bus-off recovery controller
│   └── dbc_decoder.h       # Unchanged: DBC parsing / signal decoding
├── src/
│   ├── main.c              # Unchanged CLI/dashboard/logging loop
│   ├── dbc_decoder.c       # Unchanged: Vector DBC parser/decoder
│   ├── core/
│   │   ├── can_parser.c    # Bus handle: socket + mock frame generator
│   │   ├── can_frame.c     # Frame wire format, validation, checksums
│   │   ├── can_core.c      # DLC/id helpers, version string
│   │   └── can_filters.c   # Filter set matching and routing
│   ├── hardware/
│   │   └── mcp2515_spi.c   # MCP2515 driver with software register model
│   ├── network/
│   │   └── vcan.c          # SocketCAN socket ops, poll loop, routing
│   └── state/
│       ├── error_machine.c     # Fault-confinement error-recovery FSM
│       └── bus_off_recovery.c  # Bus-off recovery state machine
├── tests/
│   ├── test_parser.c         # Frame/DLC/checksum/filter/vcan-routing tests
│   ├── test_state_machine.c  # Error FSM + bus-off recovery tests
│   └── test_decoder.c        # DBC decoder + mock bus regression tests
└── build/                  # Build artifacts (gitignored)
```

## 4. Module responsibilities

| Layer | Files | Responsibility |
|-------|-------|----------------|
| Core | `include/can_core.h`, `can_frame.h`, `can_filters.h`, `error_codes.h` + `src/core/*.c` | CAN identifiers, DLC bounds, frame wire serialization, CRC-8 checksums, acceptance filter matching, canonical error codes |
| Bus acquisition | `include/can_parser.h` + `src/core/can_parser.c` | `can_bus_t` handle: SocketCAN binding with automatic mock fallback; mock automotive telemetry generator with per-frame CRC-8 checksums |
| Hardware | `include/mcp2515.h` + `src/hardware/mcp2515_spi.c` | MCP2515 SPI register map, init sequence, bit-timing (CNF1/2/3), acceptance-filter mask configuration; pluggable SPI transport with a software register model for host testing |
| Network | `include/vcan.h` + `src/network/vcan.c` | Virtual CAN socket open/bind/read/write, non-blocking I/O, blocking poll loop with frame handler dispatch, filter-based frame routing |
| State | `include/error_machine.h`, `bus_off_recovery.h` + `src/state/*.c` | Fault-confinement error-recovery FSM (warning → passive → bus-off → reset → recovering) and ISO 11898-1 bus-off recovery (128 recessive-bit sequences) |
| App | `src/main.c`, `src/dbc_decoder.*` | CLI, live dashboard, CSV logging, DBC decoding (unchanged behavior) |

## 5. Error codes (`include/error_codes.h`)

All layer functions return `can_error_t` (negative = failure, `CAN_OK = 0`):

| Code | Value | Meaning |
|------|-------|---------|
| CAN_OK | 0 | Success |
| CAN_ERR_NOMEM | -1 | Allocation failure |
| CAN_ERR_IO | -2 | I/O / bus communication failure |
| CAN_ERR_TIMEOUT | -3 | Operation timed out |
| CAN_ERR_INVALID_ARG | -4 | Invalid argument |
| CAN_ERR_INVALID_DLC | -5 | DLC outside [0..8] |
| CAN_ERR_CHECKSUM | -6 | Payload checksum mismatch |
| CAN_ERR_BUFFER_OVERFLOW | -7 | Socket/buffer overflow |
| CAN_ERR_SPI_FAULT | -8 | MCP2515 SPI fault |
| CAN_ERR_BUS_OFF | -9 | Controller bus-off |
| CAN_ERR_NOT_INIT | -10 | Handle not initialized |
| CAN_ERR_NO_MESSAGE | -11 | No message available |
| CAN_ERR_UNSUPPORTED | -12 | Not supported on this platform |
| CAN_ERR_INTERNAL | -13 | Internal consistency failure |

Use `can_error_string()` for user-facing diagnostics.

## 6. DLC bounds checking and checksums

- `can_dlc_is_valid(dlc)` — true iff `0 <= dlc <= 8` (classical CAN).
- `can_dlc_to_bytes(dlc)` — byte count, or `CAN_ERR_INVALID_DLC`.
- `can_bytes_to_dlc(len)` — DLC for a byte count, or `-1` if `len > 8`.
- `can_dlc_to_length(dlc)` — identity for 0..8; ISO 15765-2 CAN FD mapping
  (9→12, 10→16, 11→20, 12→24, 13→32, 14→48, 15→64) for 9..15.
- `can_checksum_compute(data, len)` / `can_checksum_append(data, len)` /
  `can_checksum_validate(data, len, checksum)` — CRC-8 (poly 0x07, init 0x00).

The mock generator appends a CRC-8 over payload bytes 0..6 into byte 7 of
every 8-byte frame; `can_bus_read` exposes `checksum`/`checksum_valid` on each
message so consumers can enforce payload integrity.

## 7. MCP2515 SPI abstraction

`include/mcp2515.h` defines the full register subset used by the driver
(CANSTAT, CANCTRL, CNF1/2/3, TEC, REC, RX filter/mask registers, TX/RX
control), SPI command opcodes, operating modes (CONFIG, NORMAL, SLEEP,
LOOPBACK, LISTEN-ONLY) and bit-timing parameters.

`src/hardware/mcp2515_spi.c` implements:
- `mcp2515_create()`/`destroy()` — pluggable SPI transport via
  `mcp2515_spi_ops_t` (read/write/read-burst/write-burst callbacks). When no
  transport is supplied, an internal software register model is used, so the
  driver is fully functional and unit-testable on any host.
- `mcp2515_reset()` — software reset + default register restore.
- `mcp2515_set_mode()` — polls CANSTAT until the mode change is acknowledged.
- `mcp2515_set_bitrate()` — computes CNF1/2/3 for a target bit rate (default
  16 MHz oscillator).
- `mcp2515_configure_acceptance_filters()` — programs RXF0/RXF1 and RXM0/RXM1
  from a `can_filter_set_t` (only valid in CONFIG mode).
- `mcp2515_error_counters()` / `mcp2515_is_bus_off()` — TEC/REC monitoring;
  bus-off when TEC > 255 per the CAN specification.

## 8. Virtual CAN (`vcan`) network layer

`include/vcan.h` + `src/network/vcan.c` provide `vcan_socket_t` and the open/
bind/read/write/close socket operations. On Linux these wrap real
`AF_CAN`/`CAN_RAW` SocketCAN sockets with `select()`-based polling; on other
platforms every operation reports `CAN_ERR_UNSUPPORTED` so the same application
code compiles unchanged.

The layer also supplies platform-independent frame routing
(`vcan_route_frame()`, built on the acceptance filter set) and a blocking poll
loop (`vcan_poll_loop()`) that dispatches received frames to a caller-provided
handler until it returns non-zero or the caller-supplied running flag is
cleared.

## 9. Build system

The Makefile builds flat objects into `build/` (kept out of source trees and
gitignored). Targets: `all`, `test`, `run`, `clean`, `prepare`. `CC` defaults
to `gcc` while remaining overridable from the environment or command line.
Each `tests/test_*.c` is a standalone test binary with its own `main()`.

## 10. Compatibility

- The `can_parser.h` public API (`can_bus_init`, `can_bus_read`,
  `can_bus_is_mock`, `can_bus_get_interface`, `can_bus_close`,
  `can_msg_to_hex_string`) is preserved, so `main.c` and `tests/test_decoder.c`
  required no behavioral changes (`can_bus_last_error()` was added).
- `can_msg_t` is now defined in `include/can_frame.h` and re-exported through
  `can_parser.h`; `can_parser.c` converts wire frames into host messages with
  DLC clamping and identifier flag stripping.
- DBC decoder (`src/dbc_decoder.*`) and its tests are untouched.

## 11. Verification

```bash
make all        # compiles all layers with -Wall -Wextra -std=gnu99 -O2, zero warnings
make test       # runs parser, state-machine and decoder test suites
make run        # runs the logger (mock mode when no SocketCAN interface exists)
```

All test suites pass on a host without CAN drivers or root privileges.
