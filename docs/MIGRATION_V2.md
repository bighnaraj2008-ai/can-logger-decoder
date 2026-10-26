# Version Migration Guide — Upgrading from v1.x to v2.0

`can-logger-decoder` v2.0.0 · Day 3 Migration Architecture

This guide assists developers and embedded systems engineers in migrating existing applications, scripts, and test environments from `can-logger-decoder` v1.x to v2.0.0.

---

## 1. Summary of Breaking Architectural Changes

| Feature Area | v1.x Monolith | v2.0.0 Modular Architecture |
|:-------------|:--------------|:----------------------------|
| **Header Structure** | Single `can_decoder.h` | Decoupled headers: `can_core.h`, `can_parser.h`, `mcp2515.h`, `vcan_interface.h`, `error_codes.h` |
| **Error Handling** | Generic integer return codes (`-1`/`0`) | Strongly-typed `can_error_t` enumeration with canonical strings |
| **Fault Confinement**| Basic print statements | Formal ISO 11898-1 Error FSM (`src/state/error_machine.c`) with automatic bus-off recovery |
| **Hardware Abstraction**| Hardcoded SPI transfers | Pluggable `mcp2515_spi_ops_t` interface supporting hardware SPI, mocks, and simulators |
| **Test Fixtures** | Ad-hoc text strings | Structured binary (`.bin`) and hex (`.hex`) fixtures with cryptographic checksum verification |

---

## 2. Header Inclusion Updates

### Before (v1.x)
```c
#include "can_decoder.h"
```

### After (v2.0.0)
Include only the specific layer headers required by your module:
```c
// Core frame structures, DLC validators, and error codes
#include "can_core.h"
#include "error_codes.h"

// Bus acquisition handle (SocketCAN / Mock fallback)
#include "can_parser.h"

// DBC signal unpacking
#include "dbc_decoder.h"

// If interfacing with physical MCP2515 hardware
#include "mcp2515.h"

// If managing raw SocketCAN sockets
#include "vcan_interface.h"
```

---

## 3. Return Code Handling Migration

In v1.x, functions returned `0` for success or `-1` on generic error. In v2.0, all functions return `can_error_t`:

```c
can_error_t err = can_frame_validate(&frame);
if (err != CAN_OK) {
    fprintf(stderr, "Frame error: %s (code %d)\n", can_error_string(err), err);
    // Handle error according to error code
}
```

---

## 4. CSV Schema Additions

In v2.0.0, `can_log.csv` includes high-precision nanosecond monotonic timestamps (`Timestamp_Sec`) alongside standard ISO-8601 UTC strings (`Timestamp_ISO`). Automated downstream parsers should update column indexes accordingly or parse by column header name.
