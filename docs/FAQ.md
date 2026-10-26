# Frequently Asked Questions — CAN Bus Logger & Decoder

`can-logger-decoder` v2.0.0 · Day 3 FAQ

Answers to common questions regarding hardware interfacing, Linux SocketCAN, DBC file parsing, and error confinement.

---

## 1. General & Architecture

### Q: Does `can-logger-decoder` support CAN FD?
**A:** No. `can-logger-decoder` is built for Classical CAN 2.0A and 2.0B (0–8 data bytes). Frames with DLC > 8 are safely clamped or rejected with `CAN_ERR_INVALID_DLC`.

### Q: How does fallback mock mode work without physical hardware?
**A:** When running on non-Linux systems (Windows, macOS) or when no SocketCAN interface is specified, the application automatically activates an internal physics-based vehicle simulation engine (`src/core/can_parser.c`), generating synthetic engine speed, throttle, and temperature telemetry frames with valid ISO timestamps.

---

## 2. Hardware & Interfacing

### Q: Why does my MCP2515 fail to enter NORMAL mode?
**A:** The MCP2515 requires a valid clock source and a responsive SPI bus to transition between modes:
1. Verify the module crystal frequency (8 MHz vs 16 MHz) matches `MCP2515_DEFAULT_OSC_HZ`.
2. Verify `/RESET` is pulled HIGH (to VDD) with a 10 kΩ resistor.
3. Check that the CAN bus has 120 Ω termination enabled; without recessive/dominant bit feedback, the controller may immediately flag transmit errors.

### Q: Can I connect a 5 V MCP2515 directly to a 3.3 V host MCU?
**A:** Powering the MCP2515 at 5 V means its `SO` (MISO) pin outputs 5 V logic, which can permanently damage 3.3 V non-tolerant host GPIOs (like ESP32 or Raspberry Pi). Use a level shifter or 1 kΩ / 2 kΩ resistor voltage divider on `SO`.

---

## 3. Linux SocketCAN

### Q: Why do I get "Permission Denied" when creating `vcan0`?
**A:** Creating kernel network interfaces requires root privileges:
```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```
Once the interface is up, `can-logger-decoder` can read from it without root if read permissions are granted.

---

## 4. Signal Decoding & DBC

### Q: How are signed two's complement signals decoded?
**A:** When a signal definition in `.dbc` specifies the `-` sign flag, the unpacked raw bit pattern is sign-extended using the signal bit length before applying the linear transformation $(R \times \text{Scale}) + \text{Offset}$.
