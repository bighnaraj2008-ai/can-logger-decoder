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
  `data[] array is not populated.
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

---

## 4. Data Length Code (DLC) rules

The DLC is a 4-bit field in the control field. For classical CAN it maps 1:1
to a byte count of 0–8.

### 4.1 DLC-to-byte mapping

| DLC | Data bytes | DLC | Data bytes |
|:----|:-----------|:----|:-----------|
| 0 | 0 | 4 | 4 |
| 1 | 1 | 5 | 5 |
| 2 | 2 | 6 | 6 |
| 3 | 3 | 7 | 7 |
| — | — | 8 | 8 |

### 4.2 Validation helpers (`include/can_core.h`, `src/core/dlc_validator.c`)

| Function | Behaviour |
|:---------|:----------|
| `can_dlc_is_valid(dlc)` | `true` iff `0 <= dlc <= 8` |
| `can_dlc_to_bytes(dlc)` | Byte count for a DLC, or `CAN_ERR_INVALID_DLC` |
| `can_bytes_to_dlc(len)` | DLC for a byte count, or `-1` if `len > 8` |
| `can_dlc_to_length(dlc)` | Identity for 0–8; ISO 15765-2 CAN FD mapping for 9–15 (9→12 … 15→64), clamped for validation |

The parser rejects any frame with `dlc > 8` and reports `CAN_ERR_INVALID_DLC`.
The mock generator always emits valid DLCs (1–8) with an appended CRC-8 byte
when the frame is 8 bytes long.

---

## 5. Payload parsing & DBC signal decoding

Once a frame passes DLC and identifier validation, the DBC decoder maps its
payload bytes to named engineering signals.

### 5.1 Signal bit layout (Vector DBC convention)

Each signal is defined by:
- `start_bit` — bit index within the payload
- `bit_len` — width in bits (1–64)
- `endian` — `CAN_ENDIAN_LITTLE` (Intel, `@1`) or `CAN_ENDIAN_BIG` (Motorola, `@0`)
- `sign` — `CAN_SIGN_UNSIGNED` (`+`) or `CAN_SIGN_SIGNED` (`-`, two's complement)

#### Intel / Little-Endian (`@1`)
Bit numbering starts at the **LSB of byte 0** and increases toward the MSB of
the highest byte. A signal may cross byte boundaries.

#### Motorola / Big-Endian (`@0`)
Bit numbering starts at the **MSB of byte 0** and decreases through the LSB of
byte 0, then continues to the MSB of byte 1, and so on. A signal may cross byte
boundaries.

### 5.2 Decoding formula

For an extracted raw integer `R`:

```
Physical Value = (R × Scale) + Offset
```

- `Scale` and `Offset` come from the DBC `SG_` definition.
- Signed values use two's complement interpretation of the extracted bit field.
- Result is clamped / range-checked against DBC `[Min | Max]`; the decoder sets
  `in_range = false` when outside bounds.

### 5.3 Worked example (from `sample.dbc`)

`BO_ 256 Engine_Data: 8 Engine_ECU`
`SG_ EngineRPM : 0|16@1+ (0.25,0) [0|16383.75] "rpm" Dashboard_Display`

Payload bytes: `10 27 72 1F 7F 82 AA 55`

- `EngineRPM` starts at bit 0, length 16, little-endian, unsigned.
- Raw 16-bit value (bytes 0–1, LSB first): `0x2710` = 10000.
- Physical: `10000 × 0.25 + 0 = 2500.0 rpm`. In range `[OK]`.

### 5.4 DBC syntax reference

Message definition:
```
BO_ <MessageID_Dec> <MessageName>: <DLC> <TransmitterNode>
```

Signal definition:
```
SG_ <SignalName> : <StartBit>|<Length>@<Endian><Sign> (<Scale>,<Offset>) [<Min>|<Max>] "<Unit>" <ReceiverNodes>
```

| Field | Meaning | Example |
|:------|:--------|:--------|
| `<MessageID_Dec>` | CAN identifier, **decimal** | `256` (= `0x100`) |
| `<DLC>` | Data length in bytes (1–8) | `8` |
| `<StartBit>` | LSB bit index (Intel) / MSB (Motorola) | `0` |
| `<Length>` | Bit field width | `16` |
| `<Endian>` | `1` Intel little, `0` Motorola big | `1` |
| `<Sign>` | `+` unsigned, `-` signed two's complement | `+` |
| `(<Scale>,<Offset>)` | Linear scaling coefficients | `(0.25,0)` |
| `[<Min>|<Max>]` | Engineering bounds | `[0|16383.75]` |
| `"<Unit>"` | Unit string | `"rpm"` |

For a full walkthrough, see the **DBC File Format Guide** in `README.md` and
`include/dbc_decoder.h`.

---

## 6. Frame ingestion pipeline

Raw bytes arrive from one of three sources (see `docs/ARCHITECTURE.mmd`):

1. **Physical CAN** — MCP2515 over SPI (`src/hardware/`).
2. **Virtual CAN** — Linux SocketCAN `vcan0` / `can0` (`src/network/`).
3. **Mock telemetry** — built-in physics simulator fallback (`src/core/can_parser.c`).

Each source yields a `can_frame_t`, which the parser converts into a
host-order `can_msg_t`:

```
can_frame_t (wire)
   ↓  strip flag bits, derive is_extended/is_rtr/is_error
can_msg_t (host)
   ↓  validate DLC (0–8), compute ISO-8601 + monotonic timestamp
   ↓  error-machine event (RX_OK / ERROR_FRAME / BUS_OFF …)
   ↓  DBC match by clean CAN ID
   ↓  signal unpack (Intel/Motorola, sign, scale+offset)
   ↓  logger output (CSV row + dashboard row)
```

Data structures:

**`can_frame_t`** (`include/can_core.h`) — on-the-wire / controller-native:
- `can_id` (raw, may include flag bits)
- `can_dlc` (raw DLC)
- `data[8]`
- `is_extended`, `is_rtr`, `is_error` (derived)
- `checksum`, `checksum_valid`

**`can_msg_t`** (`include/can_parser.h` / `can_core.h`) — host-order message:
- `can_id` (clean, flags stripped)
- `dlc` (validated, 0–8)
- `data[8]`
- `timestamp_sec` (monotonic seconds)
- `timestamp_iso[64]` (ISO-8601 UTC)
- `is_extended`, `is_rtr`, `is_error`
- `checksum`, `checksum_valid`

### 6.1 Error handling in parsing

| Condition | Result | Error code |
|:----------|:-------|:-----------|
| DLC > 8 | Rejected, not delivered | `CAN_ERR_INVALID_DLC` |
| Checksum mismatch (mock frames) | Flagged `checksum_valid = false` | `CAN_ERR_CHECKSUM` |
| No message available (non-blocking) | `can_bus_read()` returns 0 | `CAN_ERR_NO_MESSAGE` |
| Socket / SPI read failure | `can_bus_read()` returns -1 | `CAN_ERR_IO` / `CAN_ERR_SPI_FAULT` |
| Buffer overflow | State machine `EVT_SOCKET_OVERFLOW` | `CAN_ERR_BUFFER_OVERFLOW` |
| Bus-off (TEC > 255) | State machine `ERR_STATE_BUS_OFF` | `CAN_ERR_BUS_OFF` |

All layer functions return `can_error_t` (negative = failure, `CAN_OK = 0`);
see `include/error_codes.h` and `can_error_string()`.

---

## 7. Cross-reference

| Topic | Document |
|:------|:---------|
| MCP2515 SPI wiring & configuration | `docs/HARDWARE_SETUP.md` |
| Register map, commands, modes, bit timing | `include/mcp2515.h` |
| Error confinement FSM & bus-off recovery | `docs/STATE_MACHINE.md` |
| DBC parser / signal decode API | `include/dbc_decoder.h` |
| Error codes | `include/error_codes.h` |
| System architecture & data-flow diagrams | `docs/ARCHITECTURE.mmd` |
| Frame fixtures (raw / extended / corrupted) | `tests/fixtures/` |