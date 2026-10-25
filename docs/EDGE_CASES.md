# CAN Bus Edge Cases & Failure Recovery Playbooks

This guide details the edge cases handled by `can-logger-decoder`, explaining the failure modes, underlying standards, and recovery playbooks.

---

## 1. Data Length Code (DLC) Bounds & Mismatches

### Specification & Rules (ISO 11898-1)
In Classical CAN 2.0A and 2.0B, legal DLC values range from `0` to `8` bytes. Any frame received with a DLC greater than 8 in classical mode violates the specification and must be rejected to prevent buffer overflow vulnerabilities.

### Edge Conditions Handled
- **Out-of-range DLC values (`DLC >= 9`)**: Detected during wire parsing; returns `CAN_ERR_INVALID_DLC`.
- **Payload Truncation**: When the wire buffer terminates prematurely before the declared DLC bytes can be read; returns `CAN_ERR_IO`.
- **Header Truncation**: When fewer than 5 bytes (4 ID + 1 DLC) are provided; returns `CAN_ERR_IO`.
- **Zero-length Frames (`DLC = 0`)**: Fully supported per CAN specification (used for synchronization and heartbeat pings).

### Recovery Playbook
1. If the bus receives high rates of `CAN_ERR_INVALID_DLC`, inspect the transmitting node configuration (ensure CAN FD frames are not being routed to a classical transceiver).
2. The parser automatically discards corrupt frames without crashing and increments the `rx_frame_drop_count` telemetry metric.

---

## 2. Invalid Arbitration Identifiers & Flag Masking

### Specification & Rules
- Standard Frame Format (SFF): 11-bit identifier (`0x000` to `0x7FF`).
- Extended Frame Format (EFF): 29-bit identifier (`0x00000000` to `0x1FFFFFFF`).
- Special Flags:
  - `CAN_EFF_FLAG` (`0x80000000`): Distinguishes extended identifiers.
  - `CAN_RTR_FLAG` (`0x40000000`): Remote Transmission Request.
  - `CAN_ERR_FLAG` (`0x20000000`): Error frame indicator.

### Edge Conditions Handled
- **High-Bit Normalization**: Identifiers with stray bits above `0x1FFFFFFF` are masked down using `can_id_clean()` to preserve message integrity.
- **Flag Cleansing**: When querying DBC signals, control flags are stripped cleanly to match raw arbitration IDs against database message definitions (`BO_`).

---

## 3. Bus-Off Fault Confinement & Automatic Recovery

### Specification & Rules (ISO 11898-1)
CAN controllers track transmission and reception reliability using error counters:
- **Transmitter Error Count (TEC)**
- **Receiver Error Count (REC)**

Fault confinement states:
- **Error Active**: `TEC < 96` and `REC < 96` (normal operation).
- **Error Warning**: `TEC >= 96` or `REC >= 96` (`ERR_STATE_WARNING`).
- **Error Passive**: `TEC > 127` or `REC > 127` (`ERR_STATE_ERROR_PASSIVE`).
- **Bus-Off**: `TEC > 255` (`ERR_STATE_BUS_OFF`) — controller disconnects from bus.

### Recovery Sequence
1. Upon entering `ERR_STATE_BUS_OFF`, transmission ceases immediately.
2. The recovery controller transitions to `BOR_WAITING_BUS_IDLE`.
3. It waits for the bus to remain idle for 128 occurrences of 11 consecutive recessive bits.
4. Once verified, `EVT_RESET_COMPLETE` is triggered, clearing error counters and returning the state machine to `ERR_STATE_NORMAL`.

---

## 4. Socket Buffer Overflow Recovery

### Congestion Mitigation
- When Linux socket receive buffers or internal ring buffers fill up faster than the decoder can consume them, `EVT_SOCKET_OVERFLOW` is dispatched.
- The state machine enters `ERR_STATE_WARNING` without fatal termination.
- Telemetry metrics increment `fault_count`.
- Once subsequent clean frames (`EVT_RX_OK` / `EVT_TX_OK`) are processed, the system self-heals back to `ERR_STATE_NORMAL`.
