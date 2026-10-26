# Incident Playbook — Bus-Off Errors & Buffer Overflows

`can-logger-decoder` v2.0.0 · Day 3 Operational Playbook

This playbook provides step-by-step procedures for detecting, diagnosing, and mitigating operational emergencies in production CAN logger deployments.

---

## 1. Incident Severity Classification

| Severity Level | Operational Impact | Immediate Action |
|:---------------|:-------------------|:-----------------|
| **SEV-1 (Critical)** | Node enters persistent `BUS_OFF`; zero frames transmitted or received | Engage hardware auto-recovery; restart transceiver |
| **SEV-2 (High)** | SocketCAN buffer overflow (`ENOBUFS`); telemetry frames dropped | Expand kernel receive queues; throttle non-critical logging |
| **SEV-3 (Medium)** | Spikes in CRC / Stuffing error frames (`ERROR_PASSIVE`) | Inspect physical termination and probe for EMI sources |
| **SEV-4 (Low)** | Checksum failure on simulated test packets | Re-validate test fixture seed parameters |

---

## 2. SEV-1 Playbook: Persistent Bus-Off State

### Symptoms
- Console reports `[ERROR] Node entered BUS_OFF state (TEC > 255)`.
- Linux `ip link show can0` reports `state BUS-OFF`.
- Telemetry logging halts completely.

### Root Cause Analysis
1. **Baud Rate Mismatch**: Host configured for 250 kbps while bus runs at 500 kbps. Every transmitted frame triggers bit error flags.
2. **Missing 120 Ω Termination**: Differential reflection distorts bit transitions.
3. **Severed or Shorted Bus Lines**: `CAN_H` shorted to ground or `CAN_L`.

### Recovery Procedure
1. Check physical bus resistance:
   ```bash
   # Multimeter across CAN_H and CAN_L (power off) -> Must read ~60 Ω
   ```
2. Check automatic recovery status:
   `src/state/bus_off_recovery.c` attempts recovery after 128 sequences of 11 consecutive recessive bits.
3. Manual interface recovery:
   ```bash
   sudo ip link set can0 type can restart
   ```
4. If recovery fails, issue SPI controller hardware reset:
   ```bash
   sudo systemctl restart can-logger.service
   ```

---

## 3. SEV-2 Playbook: SocketCAN Buffer Overflow (`ENOBUFS`)

### Symptoms
- Application logs show `CAN_ERR_BUFFER_OVERFLOW`.
- `cat /proc/net/can/stats` shows rising `rx_overflow` count.

### Root Cause Analysis
Decoder processing thread blocked on disk I/O while receiving burst traffic (>10,000 fps).

### Mitigation & Tuning
1. Enlarge Linux kernel socket buffer maximums:
   ```bash
   sudo sysctl -w net.core.rmem_max=4194304
   sudo sysctl -w net.core.rmem_default=1048576
   ```
2. Increase interface transmission queue length:
   ```bash
   sudo ip link set vcan0 txqueuelen 5000
   ```
3. Enable buffered I/O mode in `can-logger-decoder`:
   Run with `--buffered` or pipe output through high-speed disk cache.
