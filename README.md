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
