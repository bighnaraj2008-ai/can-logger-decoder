# State Machine Reference — Error Recovery & Bus Off (`can-logger-decoder` v2.0.0)

## 1. Overview

This document describes the two cooperating state machines in `src/state/`:

1. **`error_machine.c`** — the fault-confinement error-recovery finite state
   machine (FSM). It models CAN error states (NORMAL → WARNING → ERROR_PASSIVE
   → BUS_OFF) plus explicit RESET and RECOVERING states, and reacts to bus,
   socket and SPI events.
2. **`bus_off_recovery.c`** — the bus-off recovery controller. Once bus-off is
   detected it drives the controller through bus-idle waiting, reset and the
   ISO 11898-1 recovery sequence (128 sequences of 11 consecutive recessive
   bits) before rejoining the bus.

Both modules are pure C, allocate nothing, are driven by caller-supplied events
and time, and are fully unit-testable on a host without CAN hardware.

## 2. Error-recovery FSM (`error_machine.h` / `error_machine.c`)

### 2.1 States

| State | Meaning |
|-------|---------|
| `ERR_STATE_NORMAL` | Bus healthy; error counters within limits. |
| `ERR_STATE_WARNING` | TEC or REC ≥ 96 (error warning). Still healthy. |
| `ERR_STATE_ERROR_PASSIVE` | TEC or REC ≥ 127 (error passive). |
| `ERR_STATE_BUS_OFF` | TEC > 255; the node is off the bus. Not healthy. |
| `ERR_STATE_RECOVERING` | Bus-off recovery sequence in progress. |
| `ERR_STATE_RESET` | Controller reset requested / in progress. |

### 2.2 Events

| Event | Meaning |
|-------|---------|
| `EVT_RX_OK` / `EVT_TX_OK` | Frame received/transmitted without error; decay counters. |
| `EVT_ERROR_FRAME` | Error frame detected; increment TEC/REC, re-evaluate thresholds. |
| `EVT_SPI_FAULT` | MCP2515 SPI communication fault; request controller reset. |
| `EVT_SOCKET_OVERFLOW` / `EVT_RX_BUFFER_OVERFLOW` | SocketCAN or internal queue overflow; congestion fault → WARNING. |
| `EVT_BUS_OFF_DETECTED` | Controller reported bus-off (TEC > 255). Enter BUS_OFF. |
| `EVT_RECOVERY_TIMEOUT` | Recovery sequence timed out. |
| `EVT_RESET_COMPLETE` | Controller reset finished; clear counters, return to NORMAL. |
| `EVT_MANUAL_RESET` | Operator-initiated reset from any state. |

### 2.3 Fault-confinement thresholds

These mirror the CAN specification and are defined in `error_machine.h`:

- `ERR_CONF_WARNING_THRESHOLD  96` — error warning
- `ERR_CONF_PASSIVE_THRESHOLD 127` — error passive
- `ERR_CONF_BUS_OFF_LIMIT     256` — bus-off (TEC > 255)

`error_machine_inject_error(m, tx_inc, rx_inc)` increments both counters and
re-evaluates the state, promoting WARNING → ERROR_PASSIVE → BUS_OFF as
thresholds are crossed. `EVT_RX_OK`/`EVT_TX_OK` decay the counters, modeling
error recovery on an idle bus.

### 2.4 Transition summary

| From | Event | To | Action |
|------|-------|----|--------|
| NORMAL | RX_OK / TX_OK | NORMAL | Decay counters |
| NORMAL | ERROR_FRAME | WARNING / ERROR_PASSIVE / BUS_OFF | Increment counters, promote by thresholds |
| NORMAL | SPI_FAULT | RESET | `fault_count++`, `on_enter_reset()` callback |
| NORMAL | SOCKET_OVERFLOW / RX_BUFFER_OVERFLOW | WARNING | `fault_count++` |
| NORMAL | BUS_OFF_DETECTED | BUS_OFF | `on_enter_bus_off()` callback |
| NORMAL | MANUAL_RESET | RESET | `on_enter_reset()` callback |
| WARNING | RX_OK / TX_OK | NORMAL (when counters < 96) | Decay counters |
| WARNING | ERROR_FRAME | ERROR_PASSIVE (when ≥ 127) | Increment counters |
| WARNING | SPI_FAULT | RESET | fault handling |
| WARNING | BUS_OFF_DETECTED | BUS_OFF | `on_enter_bus_off()` callback |
| ERROR_PASSIVE | RX_OK / TX_OK | NORMAL / WARNING | Decay counters, demote when below thresholds |
| ERROR_PASSIVE | BUS_OFF_DETECTED | BUS_OFF | `on_enter_bus_off()` callback |
| BUS_OFF | RECOVERY_TIMEOUT (auto) | RECOVERING | Start recovery sequence |
| BUS_OFF | RESET_COMPLETE | NORMAL | Clear counters, `on_recovered()` callback |
| BUS_OFF | MANUAL_RESET | RESET | `on_enter_reset()` callback |
| RECOVERING | RESET_COMPLETE | NORMAL | Clear counters, `recovery_count++`, `on_recovered()` callback |
| RECOVERING | RECOVERY_TIMEOUT | BUS_OFF | Retry recovery |
| RESET | RESET_COMPLETE | NORMAL | Clear counters, `recovery_count++`, `on_recovered()` callback |

### 2.5 Callbacks

- `on_enter_bus_off(ctx)` — fired on entry to BUS_OFF.
- `on_enter_reset(ctx)` — fired on entry to RESET.
- `on_recovered(ctx)` — fired when the machine returns to NORMAL after a
  failure state.

Use `error_machine_is_bus_off()` / `error_machine_is_healthy()` for queries,
and `err_state_to_string()` for logging.

## 3. Bus-off recovery controller (`bus_off_recovery.h` / `bus_off_recovery.c`)

### 3.1 States

| State | Meaning |
|-------|---------|
| `BOR_IDLE` | Not active. |
| `BOR_WAITING_BUS_IDLE` | Waiting for `wait_for_bus_idle()` to report the bus idle. |
| `BOR_RESET_ISSUED` | Reset command sent to the controller. |
| `BOR_RECOVERING` | Observing the 128 recovery sequences. |
| `BOR_RECOVERED` | Successfully rejoined the bus. |
| `BOR_FAILED` | Recovery gave up (reset attempts exhausted or timeout). |

### 3.2 Operation

The controller is driven by repeated calls to
`bus_off_recovery_poll(rec, now_ms)` with a monotonic millisecond time source
supplied by the caller — this keeps the module deterministic under test.

1. Call `bus_off_recovery_start()` when BUS_OFF is detected. The controller
   enters `BOR_WAITING_BUS_IDLE`.
2. On each poll it invokes `wait_for_bus_idle(ctx, timeout_ms)` (platform
   callback). While the bus is busy it keeps waiting, aborting to `BOR_FAILED`
   when the overall `timeout_ms` budget elapses.
3. Once the bus is idle it invokes `reset_controller(ctx)`. On failure it
   retries up to `max_attempts` (default 8); persistent failure → `BOR_FAILED`.
4. On reset success it enters `BOR_RECOVERING` and credits the passage of time
   against the 128-sequence recovery window (`BUS_OFF_RECOVERY_SEQUENCES`);
   when the window elapses → `BOR_RECOVERED`.

### 3.3 Callbacks

- `reset_controller(ctx)` — issue a controller reset; returns 0 on success.
- `wait_for_bus_idle(ctx, timeout_ms)` — wait for bus idle; return 1 when
  idle, 0 to keep waiting, < 0 on error.

Query helpers: `bus_off_recovery_state()`, `bus_off_recovery_is_done()`,
`bus_off_recovery_has_failed()`, `bor_state_to_string()`.

## 4. SPI fault handling

An `EVT_SPI_FAULT` (MCP2515 SPI read/write/bit-modify failure) is treated as a
reset-level fault from any state:

- NORMAL / WARNING / ERROR_PASSIVE → `ERR_STATE_RESET`, `fault_count++`,
  `on_enter_reset()` invoked so the application can re-initialize the
  controller (registers, bit timing, filters).
- BUS_OFF → fault counted, state retained (recovery is already in progress).
- RESET / RECOVERING → fault counted.

The driver surfaces SPI faults through `mcp2515_read_reg`/`write_reg`/
`bit_modify` return codes (`CAN_ERR_SPI_FAULT`-compatible negatives); the
application translates them into `EVT_SPI_FAULT` events for the error machine.

## 5. Socket buffer overflow handling

`EVT_SOCKET_OVERFLOW` and `EVT_RX_BUFFER_OVERFLOW` are congestion faults, not
fatal bus errors:

- NORMAL → WARNING, `fault_count++`, continue operating.
- WARNING → remain WARNING, `fault_count++`.
- The receiver should drain the socket queue (e.g. keep calling
  `vcan_socket_read_frame()` until `CAN_ERR_NO_MESSAGE`/timeout) to relieve
  backpressure.

## 6. Usage example

```c
error_machine_t err;
error_machine_init(&err);
err.on_enter_bus_off = my_bus_off_handler;
err.on_enter_reset   = my_reset_handler;
err.on_recovered     = my_recovered_handler;

/* inside the receive loop */
if (spi_read_failed) {
    error_machine_process_event(&err, EVT_SPI_FAULT);
} else if (err_frame_seen) {
    error_machine_process_event(&err, EVT_ERROR_FRAME);
} else if (socket_overflow) {
    error_machine_process_event(&err, EVT_SOCKET_OVERFLOW);
}

if (error_machine_is_bus_off(&err)) {
    bus_off_recovery_t bor;
    bus_off_recovery_init(&bor);
    bor.reset_controller = mcp2515_reset_wrapper;
    bor.wait_for_bus_idle = mcp2515_wait_bus_idle;
    bus_off_recovery_start(&bor);

    uint64_t now = millis();
    while (!bus_off_recovery_is_done(&bor) &&
           !bus_off_recovery_has_failed(&bor)) {
        now += 10;
        bus_off_recovery_poll(&bor, now);
    }
}
```

## 7. Testing

`tests/test_state_machine.c` covers:

- Initial state, idempotent reset, argument validation (NULL handles,
  out-of-range events).
- Fault confinement: 100 TX errors → WARNING, 200 → ERROR_PASSIVE,
  300 → BUS_OFF; `is_healthy()` / `is_bus_off()` reflect this.
- Event transitions: RX_OK/TX_OK decay, SPI_FAULT → RESET, socket/RX buffer
  overflow → WARNING, BUS_OFF_DETECTED → BUS_OFF with callback firing,
  MANUAL_RESET → RESET, RESET_COMPLETE → NORMAL with `on_recovered`.
- Bus-off recovery: happy path (IDLE → waiting → reset → recovering →
  recovered), exhausted reset attempts → FAILED, bus-idle timeout → FAILED,
  invalid-argument handling.

Run with:

```bash
make test   # builds and runs tests/test_state_machine.exe (plus the other suites)
```
