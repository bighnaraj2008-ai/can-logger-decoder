# Embedded Security Hardening & Fault Tolerance Guide

`can-logger-decoder` v2.0.0 · Day 3 Security Hardening

This guide specifies software defense mechanisms, fault-tolerant design principles, and secure compilation flags used in `can-logger-decoder` to mitigate vulnerabilities inherent to Controller Area Networks.

---

## 1. Automotive Threat Model

Classical CAN (ISO 11898-1) is an unencrypted, broadcast-based bus without cryptographic node authentication.

| Threat Category | Attack Vector | Decoder Mitigation |
|:----------------|:--------------|:-------------------|
| **Frame Spoofing** | Adversary injects frames with forged IDs | Acceptance filtering and DBC identifier boundary enforcement |
| **CAN Bus Flood (DoS)** | High-frequency dominant frame storm | Ring buffer backpressure, error state confinement (`BUS_OFF`), rate limiting |
| **Buffer Exploits** | Corrupted DLC (> 8) or malformed payload | Strict DLC validation, bounds checking, zero dynamic heap allocations in hot path |
| **Replay Attacks** | Injecting recorded valid sequences | Monotonic timestamp verification and sequence jitter tracking |

---

## 2. Memory Safety & Runtime Hardening

### 2.1 Static Allocation in Hot Path
- The frame reception and decoding loop in `can-logger-decoder` performs **zero heap allocations** (`malloc`/`free`) during active telemetry capture.
- All frame buffers (`can_frame_t`, `can_msg_t`) and signal unpack arrays are statically allocated or stack-allocated with fixed bounds.
- Prevents memory fragmentation and eliminating heap exhaustion vulnerabilities.

### 2.2 Compiler Hardening Flags
Build scripts enforce modern toolchain security flags:

```make
# Stack protection & canary verification
CFLAGS += -fstack-protector-strong

# Fortify source functions (memcpy, sprintf, etc.)
CFLAGS += -D_FORTIFY_SOURCE=2

# Format string vulnerability prevention
CFLAGS += -Wformat -Wformat-security -Werror=format-security

# Read-only relocation table & immediate binding
LDFLAGS += -Wl,-z,relro -Wl,-z,now

# Position Independent Executable (ASLR)
CFLAGS += -fPIE
LDFLAGS += -pie
```

---

## 3. Input Validation & Frame Sanitization

1. **DLC Bounds Verification**:
   ```c
   if (frame->can_dlc > 8) {
       return CAN_ERR_INVALID_DLC;
   }
   ```
2. **Identifier Range Clamping**:
   - Standard 11-bit: Rejected if `clean_id > 0x7FF`.
   - Extended 29-bit: Rejected if `clean_id > 0x1FFFFFFF`.
3. **Payload Checksum Validation**:
   - Built-in simulation frames carry CRC-8 checksum verification across payload bytes 0–6.

---

## 4. Fault Confinement & Watchdog Supervision

- **Hardware Watchdog**: Embedded deployments kick the internal watchdog timer upon each successful loop iteration. If the decoder blocks or hangs in an infinite loop, the watchdog triggers a clean hardware reset.
- **Error Confinement State Machine**: Automatically detects and transitions through WARNING, ERROR PASSIVE, and BUS OFF states per ISO 11898-1, preventing a faulty node from jamming the physical bus.
