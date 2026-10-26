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

---

## 2. MCP2515 Hardware Driver API

The Hardware Layer abstracts physical SPI communications with the Microchip MCP2515 stand-alone CAN controller.

### 2.1 Hardware Structures & Types

#### `mcp2515_spi_ops_t`
Abstracts low-level SPI bus transfer primitives:
```c
typedef struct {
    int (*spi_transfer)(const uint8_t *tx_buf, uint8_t *rx_buf, size_t len, void *user_data);
    void (*cs_select)(bool select, void *user_data);
    void *user_data;
} mcp2515_spi_ops_t;
```

#### `mcp2515_mode_t`
Controller operational states:
```c
typedef enum {
    MCP_MODE_NORMAL     = 0x00,
    MCP_MODE_SLEEP      = 0x20,
    MCP_MODE_LOOPBACK   = 0x40,
    MCP_MODE_LISTENONLY = 0x60,
    MCP_MODE_CONFIG     = 0x80
} mcp2515_mode_t;
```

### 2.2 Hardware Functions

#### `mcp2515_hw_init`
```c
can_error_t mcp2515_hw_init(mcp2515_handle_t *handle, const mcp2515_spi_ops_t *ops, uint32_t osc_freq_hz);
```
Performs software reset, validates SPI connectivity via dummy read, and places device into `MCP_MODE_CONFIG`.

#### `mcp2515_set_bitrate`
```c
can_error_t mcp2515_set_bitrate(mcp2515_handle_t *handle, uint32_t bitrate_bps);
```
Computes and programs `CNF1`, `CNF2`, and `CNF3` timing registers based on crystal frequency. Supported bitrates: 125 kbps, 250 kbps, 500 kbps, 1 Mbps.

#### `mcp2515_set_mode`
```c
can_error_t mcp2515_set_mode(mcp2515_handle_t *handle, mcp2515_mode_t mode);
```
Requests state change and polls `CANSTAT` until mode change is verified.

#### `mcp2515_configure_acceptance_filters`
```c
can_error_t mcp2515_configure_acceptance_filters(mcp2515_handle_t *handle, const can_filter_set_t *filters);
```
Programs standard/extended hardware mask (`RXM0`, `RXM1`) and filter registers (`RXF0` through `RXF5`).

#### `mcp2515_service_interrupts`
```c
can_error_t mcp2515_service_interrupts(mcp2515_handle_t *handle, can_frame_t *rx_frame, bool *frame_received);
```
Polls `CANINTF` over SPI, fetches pending RX buffers (`RXB0`, `RXB1`), handles overflow / error flags, and clears interrupt bits.

---

## 3. Virtual CAN Socket Network Interface API

The Network Layer encapsulates Linux SocketCAN (`vcan0`, `can0`) operations using Berkeley `AF_CAN` sockets.

### 3.1 Network Structures

#### `vcan_handle_t`
Encapsulates open socket descriptor and device attributes:
```c
typedef struct {
    int fd;                       /**< Socket file descriptor */
    char interface_name[16];      /**< e.g., "vcan0", "can0" */
    bool is_nonblocking;          /**< O_NONBLOCK status */
    uint32_t rx_buffer_size;      /**< Socket receive buffer size in bytes */
} vcan_handle_t;
```

### 3.2 Network Functions

#### `vcan_socket_open`
```c
can_error_t vcan_socket_open(vcan_handle_t *handle, const char *interface_name, bool nonblocking);
```
Creates `AF_CAN` `SOCK_RAW` socket, sets buffer size, and binds to network interface name via `SIOCGIFINDEX`.

#### `vcan_socket_read`
```c
can_error_t vcan_socket_read(vcan_handle_t *handle, can_frame_t *frame);
```
Reads a single `struct can_frame` from the socket into internal `can_frame_t`. Returns `CAN_ERR_NO_MESSAGE` if nonblocking and no packet is queued.

#### `vcan_socket_send`
```c
can_error_t vcan_socket_send(vcan_handle_t *handle, const can_frame_t *frame);
```
Encapsulates frame and transmits onto bus interface.

#### `vcan_socket_close`
```c
can_error_t vcan_socket_close(vcan_handle_t *handle);
```
Flushes remaining queued buffers and closes the socket descriptor.
