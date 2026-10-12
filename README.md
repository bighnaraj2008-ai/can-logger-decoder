# CAN Bus Logger & Signal Decoder (`can-logger-decoder`)

A high-performance command-line CAN (Controller Area Network) bus logger and real-time signal decoder written in portable C (GNU99).

Designed for automotive engineers, embedded systems developers, and electric vehicle (EV) telemetry analysts, `can-logger-decoder` reads raw CAN frames directly from Linux SocketCAN interfaces, parses Vector `.dbc` databases, and decodes bit-packed signals into scaled physical engineering units.

## Automotive & EV CAN Bus Background

Controller Area Network (CAN) is the broadcast vehicle bus standard designed to allow electronic control units (ECUs) to communicate without a host computer. Standard automotive systems broadcast hundreds of sensor frames across High-Speed CAN (500 kbps) buses, including:
- Powertrain metrics (Engine RPM, vehicle velocity, throttle position)
- Health diagnostics (Coolant temp, oil temp, battery rail voltage)
- Active safety (Braking hydraulic pressure, ABS intervention flags)

## Hardware Requirements & Virtual Interfaces

### Physical Hardware Options
1. **Microcontroller + CAN Controller**: Raspberry Pi / Arduino with MCP2515 SPI CAN transceiver board.
2. **USB-to-CAN Analyzers**: CandleLight, CANable, or Peak PCAN-USB adapters supporting native Linux SocketCAN `slcan` / `gs_usb`.
3. **OBD-II Interface**: OBD-II to DB9 cable connected to vehicle CAN High / Low lines.

### Virtual CAN (vcan0) Interface
When testing on workstations without hardware, Linux SocketCAN provides the `vcan` kernel module:
```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

## Compilation & Makefile Targets

The project compiles with strict standard flags: `-Wall -Wextra -std=gnu99 -O2 -Iinclude -lm`.

```bash
# Build main binary (can_decoder)
make all

# Run test suite
make test

# Run demo with 20 frames
make run

# Clean build artifacts
make clean
```

## Virtual SocketCAN (vcan0) Testing Instructions

Test frames can be injected onto `vcan0` using `can-utils`:

```bash
# Send Engine Data (ID 0x100): RPM = 2500, Speed = 80.5 km/h
cansend vcan0 100#1027721F7F820000

# Send Engine Status (ID 0x101): Coolant = 88 degC, Battery = 14.1 V
cansend vcan0 101#805F018D00000000

# Continuous traffic generator
cangen vcan0 -g 50 -I 100 -L 8
```
