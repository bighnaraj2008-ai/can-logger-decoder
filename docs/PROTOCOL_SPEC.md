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

## 2. Classical CAN frame layout (on the wire)

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

### 2.1 Arbitration field
- Starts with the identifier (MSB first). The lowest numerical identifier wins
  bus arbitration (dominant bits overwrite recessive).
- Followed by the RTR bit (dominant = data frame, recessive = remote frame)
  and the IDE bit (dominant = standard format for 11-bit).

### 2.2 Control field
- Contains the 4-bit **DLC**, which encodes the number of data bytes (0–8 for
  classical CAN). Two reserved bits (`r0`) must be recessive.

### 2.3 Data field
- 0 to 8 bytes, transmitted MSB-first within each byte.
- Bit-stuffing: after five consecutive equal bits, the transmitter inserts a
  complementary stuff bit; receivers remove it. The driver's wire-format
  converters operate on already-de-stuffed bytes delivered by the controller
  / socket.

### 2.4 CRC field (ISO 11898-1)
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
