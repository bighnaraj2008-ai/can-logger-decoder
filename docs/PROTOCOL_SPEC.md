# Protocol & Decoder Specifications — CAN Frame Formats & Payload Parsing

`can-logger-decoder` v2.0.0 · Day 3 Protocol Guide

This document specifies the CAN frame formats, identifier structures, Data
Length Code (DLC) rules, and payload parsing semantics implemented by
`can-logger-decoder`. It is the authoritative reference for the wire format
consumed from Linux SocketCAN (`can0`, `vcan0`), physical MCP2515 hardware,
and the internal mock telemetry generator.

---

## 1. Supported standards

| Feature | Support | Reference |
|:--------|:--------|:----------|
| CAN 2.0A | Standard frames (11-bit identifier) | ISO 11898-1 / CAN 2.0A |
| CAN 2.0B | Extended frames (29-bit identifier) | ISO 11898-1 / CAN 2.0B Active |
| Classical CAN data field | 0–8 bytes | CAN 2.0A/B |
| Remote frames (RTR) | Parsed & flagged | CAN 2.0A/B |
| Error frames | Parsed & flagged | ISO 11898-1 |
| CAN FD | **Not supported** (max 8 data bytes) | — |

> The decoder gracefully rejects frames whose DLC exceeds 8 (`CAN_ERR_INVALID_DLC`)
> and never attempts CAN FD decoding. CAN FD-specific DLC values (9–15) map to
> 12/16/20/24/32/48/64 bytes per ISO 15765-2 in `can_dlc_to_length()` but are
> clamped to the classical 0–8 range for actual frame validation.

---

## 2. CAN identifier structures

The raw 32-bit `can_id` carried by SocketCAN may include flag bits. The driver
strips flags and stores a clean identifier plus format flags in `can_msg_t`.

### 2.1 Standard frame format (SFF, CAN 2.0A) — 11-bit identifier

```
 Bit(s)    Field
 28        SRR (Substitute Remote Request) — always recessive (1) in standard frames
 27        IDE (Identifier Extension)      = 0  (standard format)
 26–18     r0 — reserved bits (0)
 17–0      not used
 10–0      11-bit Standard Identifier (SID)
```

- Valid identifier range: `0x000` – `0x7FF` (0 – 2047 decimal).
- Mask applied by the driver: `CAN_SFF_MASK = 0x000007FF`.
- Flag bit `CAN_EFF_FLAG (0x80000000)` is **clear** for standard frames.

### 2.2 Extended frame format (EFF, CAN 2.0B) — 29-bit identifier

```
 Bit(s)    Field
 31        FF flag (set by SocketCAN to mark an extended frame)
 30–29     SRR, IDE — IDE = 1 (extended format)
 28–0      29-bit Extended Identifier (EID)
```

- Valid identifier range: `0x00000000` – `0x1FFFFFFF` (0 – 536,870,911 decimal).
- Mask applied by the driver: `CAN_EFF_MASK = 0x1FFFFFFF`.
- Flag bit `CAN_EFF_FLAG (0x80000000)` is **set** for extended frames.

### 2.3 Identifier flag bits (SocketCAN / `can_core.h`)

| Flag | Value | Meaning |
|:-----|:------|:--------|
| `CAN_EFF_FLAG` | `0x80000000` | Frame uses extended (29-bit) format |
| `CAN_RTR_FLAG` | `0x40000000` | Remote Transmission Request frame |
| `CAN_ERR_FLAG` | `0x20000000` | Error frame (no useful data payload) |
| `CAN_SFF_MASK` | `0x000007FF` | Mask to extract 11-bit standard ID |
| `CAN_EFF_MASK` | `0x1FFFFFFF` | Mask to extract 29-bit extended ID |

Helper functions (`include/can_core.h`):
- `can_id_is_extended(raw_id)` — true when `CAN_EFF_FLAG` is set.
- `can_id_clean(raw_id)` — returns identifier with flag bits stripped.

### 2.4 Remote Transmission Request (RTR)

RTR frames request data from another node without carrying a payload.

- Flag: `CAN_RTR_FLAG (0x40000000)`.
- The `can_msg_t.is_rtr` field is set accordingly.
- RTR frames carry no data bytes; `dlc` indicates the requested length but the
  `data[]` array is not populated.
- The decoder skips DBC signal decoding for RTR frames (nothing to unpack).

### 2.5 Error frames

Error frames report bus or controller error conditions.

- Flag: `CAN_ERR_FLAG (0x20000000)`.
- The `can_msg_t.is_error` field is set.
- The error machine (`src/state/error_machine.c`) translates error-frame
  sightings into `EVT_ERROR_FRAME` events and adjusts TEC/REC confinement
  counters. See `docs/STATE_MACHINE.md`.

---

## 3. Classical CAN frame layout (on the wire)

```
 Start of Frame (SOF)        1 bit   — dominant '0', synchronizes all nodes
 Arbitration Field          11–29 bits — identifier + RTR + IDE
 Control Field               6 bits   — r0, r0, IDE (repeated), DLC (4 bits)
 Data Field                 0–64 bits — 0 to 8 data bytes
 CRC Field                  16 bits   — CRC sequence + CRC delimiter (recessive)
 ACK Field                   2 bits   — ACK slot + ACK delimiter
 End of Frame (EOF)          7 bits   — recessive
 Interframe Space (IFS)      3 bits   — recessive (interframe bus-free)
```

### 3.1 Arbitration field
- Starts with the identifier (MSB first). The lowest numerical identifier wins
  bus arbitration (dominant bits overwrite recessive).
- Followed by the RTR bit (dominant = data frame, recessive = remote frame)
  and the IDE bit (dominant = standard format for 11-bit).

### 3.2 Control field
- Contains the 4-bit **DLC**, which encodes the number of data bytes (0–8 for
  classical CAN). Two reserved bits (`r0`) must be recessive.

### 3.3 Data field
- 0 to 8 bytes, transmitted MSB-first within each byte.
- Bit-stuffing: after five consecutive equal bits, the transmitter inserts a
  complementary stuff bit; receivers remove it. The driver's wire-format
  converters operate on already-de-stuffed bytes delivered by the controller
  / socket.

### 3.4 CRC field (ISO 11898-1)
- 15-bit CRC computed over the bit stream from SOF through the end of the data
  field, using polynomial `x^15 + x^14 + x^10 + x^8 + x^7 + x^4 + x^3 + 1`.
- The controller hardware verifies the CRC on receive and generates it on
  transmit; `can-logger-decoder` trusts the controller/socket CRC result and
  flags frames with a CRC error as error frames.

> **Project-specific CRC-8:** The mock telemetry generator additionally appends
> an 8-bit CRC (poly `0x07`, init `0x00`) over payload bytes 0–6 into byte 7
> of each 8-byte frame. This is exposed via `can_msg_t.checksum` /
> `checksum_valid` and validated by `can_checksum_validate()` (`src/core/can_frame.c`).
> It is **not** part of the ISO CAN wire CRC.
