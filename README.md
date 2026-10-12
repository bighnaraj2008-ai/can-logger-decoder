# CAN Bus Logger & Signal Decoder (`can-logger-decoder`)

A high-performance command-line CAN (Controller Area Network) bus logger and real-time signal decoder written in portable C (GNU99).

Designed for automotive engineers, embedded systems developers, and electric vehicle (EV) telemetry analysts, `can-logger-decoder` reads raw CAN frames directly from Linux SocketCAN interfaces, parses Vector `.dbc` databases, and decodes bit-packed signals into scaled physical engineering units.

## Automotive & EV CAN Bus Background

Controller Area Network (CAN) is the broadcast vehicle bus standard designed to allow electronic control units (ECUs) to communicate without a host computer. Standard automotive systems broadcast hundreds of sensor frames across High-Speed CAN (500 kbps) buses, including:
- Powertrain metrics (Engine RPM, vehicle velocity, throttle position)
- Health diagnostics (Coolant temp, oil temp, battery rail voltage)
- Active safety (Braking hydraulic pressure, ABS intervention flags)
