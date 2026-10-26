# API Reference Guide — can-logger-decoder C Library

`can-logger-decoder` v2.0.0 · Day 3 API Documentation

This reference guide documents the public C API provided by the `can-logger-decoder` library. The library is organized into three distinct layers:
1. **Core Parser Layer** (`include/can_core.h`, `include/can_parser.h`)
2. **Hardware Driver Layer** (`include/mcp2515.h`)
3. **Network Socket Layer** (`include/vcan_interface.h`)

---

## 1. Core Parser & Frame API

The Core Layer defines wire-level CAN structures, validated message envelopes, and frame parsing routines.

### 1.1 Data Structures

#### `can_frame_t`
Represents a raw CAN frame as delivered by controller hardware or SocketCAN:
```c
typedef struct {
    uint32_t can_id;      /**< CAN ID + EFF/RTR/ERR flags */
    uint8_t can_dlc;      /**< Frame payload length (0-8) */
    uint8_t data[8];      /**< Raw byte array */
    bool is_extended;     /**< True if 29-bit extended ID */
    bool is_rtr;          /**< True if Remote Transmission Request */
    bool is_error;        /**< True if Error frame */
    uint8_t checksum;     /**< Optional CRC-8 payload checksum */
    bool checksum_valid;  /**< Validation flag */
} can_frame_t;
```

#### `can_msg_t`
Represents a host-order, timestamped, validated CAN message:
```c
typedef struct {
    uint32_t can_id;              /**< Clean identifier with flags stripped */
    uint8_t dlc;                  /**< Validated DLC in 0-8 range */
    uint8_t data[8];              /**< Payload data bytes */
    double timestamp_sec;         /**< Monotonic timestamp in seconds */
    char timestamp_iso[64];       /**< ISO-8601 UTC timestamp string */
    bool is_extended;             /**< Extended frame format flag */
    bool is_rtr;                  /**< RTR frame flag */
    bool is_error;                /**< Error frame flag */
    uint8_t checksum;             /**< Checksum value */
    bool checksum_valid;          /**< Checksum validity */
} can_msg_t;
```

### 1.2 Core Functions

#### `can_frame_validate`
```c
can_error_t can_frame_validate(const can_frame_t *frame);
```
Validates frame fields against CAN 2.0A/B rules.
- **Parameters:** `frame` — Pointer to incoming raw frame.
- **Returns:** `CAN_OK (0)` if valid; negative `can_error_t` on violation (`CAN_ERR_INVALID_DLC`, `CAN_ERR_INVALID_ID`).

#### `can_dlc_is_valid`
```c
bool can_dlc_is_valid(uint8_t dlc);
```
Checks if DLC is within classical CAN bounds ($0 \le dlc \le 8$).

#### `can_id_clean`
```c
uint32_t can_id_clean(uint32_t raw_id);
```
Strips `CAN_EFF_FLAG`, `CAN_RTR_FLAG`, and `CAN_ERR_FLAG` bits, returning the pure numeric arbitration ID.

#### `can_checksum_validate`
```c
bool can_checksum_validate(const can_frame_t *frame);
```
Calculates CRC-8 over bytes 0–6 and verifies against byte 7 for 8-byte frames.
