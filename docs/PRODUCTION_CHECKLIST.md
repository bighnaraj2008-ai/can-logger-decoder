# Production Deployment Readiness Checklist â€” can-logger-decoder (Day 5)

This checklist must be validated prior to deploying `can-logger-decoder`
into production automotive or industrial embedded nodes.

---

## 1. Hardware & Physical Layer
- [ ] 120 Î© bus termination verified across CAN_H and CAN_L with power off (60 Î© total).
- [ ] Logic level shifter operating correctly between 3.3V host MCU and 5V CAN transceiver.
- [ ] MCP2515 crystal oscillator frequency matches driver configuration (`MCP2515_DEFAULT_OSC_HZ`).
- [ ] Interrupt line (`/INT`) active-low connected to hardware interrupt GPIO.

## 2. Kernel & OS Configuration
- [ ] Linux SocketCAN interface enabled (`can0` up with configured bitrate).
- [ ] `txqueuelen` set to at least 1000 for burst tolerance: `ip link set can0 txqueuelen 1000`.
- [ ] Systemd service configured with `Restart=always` and watchdog timer.

## 3. Reliability & Maintenance
- [ ] Buffer rotation utility (`scripts/prune_buffer.sh`) scheduled in cron.
- [ ] Telemetry archiving policy (`docs/ARCHIVE_POLICY.md`) active.