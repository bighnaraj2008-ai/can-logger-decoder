# CAN Bus Logger & Signal Decoder (`can-logger-decoder`)

A high-performance, robust command-line CAN (Controller Area Network) bus logger and real-time signal decoder written in portable C (GNU99).

Designed for automotive engineers, embedded systems developers, and telemetry analysts, `can-logger-decoder` reads raw CAN frames directly from Linux SocketCAN interfaces (such as physical `can0` or virtual `vcan0`), parses industry-standard Vector `.dbc` databases, decodes bit-packed signals into scaled engineering units, displays a colorized live console dashboard, and logs data to structured CSV files.

When physical hardware or virtual network interfaces are not available (e.g. running outside Linux or on development workstations), the engine automatically falls back to an internal physics-based vehicle telemetry simulator.

---

## Architecture Overview

```
                      ┌──────────────────────────────────────┐
                      │        CAN Bus Input Sources         │
                      └──────────────────┬───────────────────┘
                                         │
                 ┌───────────────────────┴───────────────────────┐
                 │                                               │
       [ Real Linux SocketCAN ]                        [ Mock Telemetry Engine ]
        - Physical: can0, can1                          - Dynamic vehicle physics
        - Virtual:  vcan0, vcan1                        - Cycles standard messages
        - Kernel sys/socket & linux/can                 - High-fidelity bit packing
                 │                                               │
                 └───────────────────────┬───────────────────────┘
                                         │
                                         ▼
                      ┌──────────────────────────────────────┐
                      │    Raw Frame Ingestion (can_parser)   │
                      │  - ID masking (11-bit SFF / 29-bit EFF)│
                      │  - Monotonic & ISO-8601 timestamps    │
                      └──────────────────┬───────────────────┘
                                         │
                                         ▼
                      ┌──────────────────────────────────────┐
                      │   DBC Signal Decoder (dbc_decoder)   │
                      │  - Parser: BO_ (msg) & SG_ (signals) │
                      │  - Intel (Little) & Motorola (Big)    │
                      │  - Signed 2's Complement Unpacking   │
                      │  - Formula: Val = Raw * Scale + Off   │
                      └──────────────────┬───────────────────┘
                                         │
                         ┌───────────────┴───────────────┐
                         ▼                               ▼
            ┌─────────────────────────┐     ┌─────────────────────────┐
            │ Live Terminal Dashboard │     │  Structured CSV Logger  │
            │  - Colorized ANSI gauges│     │  - can_log.csv          │
            │  - Out-of-bounds alerts │     │  - Microsecond precision│
            └─────────────────────────┘     └─────────────────────────┘
```

---

## Directory & File Structure

```text
can-logger-decoder/
├── Makefile                # Strict build configuration (-Wall -Wextra -std=gnu99 -O2)
├── LICENSE                 # MIT Open Source License
├── .gitignore              # Ignores binaries, object files, logs, and IDE caches
├── README.md               # Detailed technical documentation and tutorial
├── sample.dbc              # Automotive DBC database (Engine, Brake, Transmission)
├── include/
│   ├── can_parser.h        # SocketCAN abstraction & mock frame generator API
│   └── dbc_decoder.h       # DBC file parsing, bit unpacking, and signal definitions
└── src/
    ├── can_parser.c        # SocketCAN socket handling, select() loop, and mock dynamics
    ├── dbc_decoder.c       # DBC file parser, Intel/Motorola unpacking, sign extension
    ├── main.c              # CLI loop, ANSI console dashboard, CSV logging, SIGINT handler
    └── test_decoder.c      # Automated unit test suite verifying math & parsing
```

---

## Key Features

1. **Native Linux SocketCAN Integration**:
   - Uses `AF_CAN`, `SOCK_RAW`, and `CAN_RAW` sockets (`sys/socket.h`, `linux/can.h`).
   - Supports Standard (11-bit) and Extended (29-bit) CAN identifiers.
   - Non-blocking I/O with `select()` timeouts for responsive termination.

2. **Zero-Hardware Mock Telemetry Fallback**:
   - Automatically detects if the requested interface is unavailable and engages a built-in mock telemetry generator.
   - Accurately simulates realistic automotive powertrain dynamics:
     - Engine RPM dynamic curves (800 - 4500 RPM)
     - Vehicle Speed correlating with gear ratio (0 - 140 km/h)
     - Throttle position, Coolant temperature thermal warm-up, Oil temperature
     - Alternator battery voltage ripple (14.1V ± 0.15V)
     - Brake application hydraulic pressures and gear shifting
   - Can also be forced anytime via the `--mock` CLI flag.

3. **Lightweight Vector DBC Parser**:
   - Reads message definitions (`BO_ <ID> <Name>: <DLC> <Sender>`).
   - Reads signal definitions (`SG_ <Name> : <Start>|<Len>@<Endian><Sign> (<Scale>,<Offset>) [<Min>|<Max>] "<Unit>"`).
   - Supports both **Intel (Little-Endian `@1`)** and **Motorola (Big-Endian `@0`)** formats.
   - Full support for signed two's complement integers (`-`) and unsigned integers (`+`).
   - Parses comments (`CM_ BO_` and `CM_ SG_`).

4. **Live Terminal Telemetry Dashboard**:
   - Real-time ANSI color-coded console output.
   - Hierarchical tree display of messages and their child signals.
   - Range validation with status tags (`[OK]` or `[OUT-OF-BOUNDS]`).

5. **Structured CSV Logging**:
   - Auto-generates `can_log.csv` with nanosecond ISO timestamps, CAN IDs (Hex & Dec), message names, raw integer payloads, scaled physical values, and engineering units.

---

## Prerequisites & Compilation

### Requirements
- **Compiler**: GCC or Clang supporting C99/GNU99.
- **Operating System**: Linux (native or WSL 2) for SocketCAN; works on any POSIX system in Mock mode.
- **Build Tool**: GNU Make.

### Build Targets

```bash
# Build the main can_decoder binary
make all

# Run the automated unit test suite
make test

# Run the application with default DBC and 20 sample frames
make run

# Clean build artifacts, binaries, and logs
make clean
```

The build uses strict flags:
`-Wall -Wextra -std=gnu99 -O2 -Iinclude -lm`

---

## Linux Virtual CAN (`vcan0`) Testing Guide

SocketCAN allows testing CAN applications on Linux without physical hardware via the `vcan` (Virtual CAN) kernel module.

### Step 1: Set Up `vcan0` on Linux / WSL

Run the following commands with superuser privileges:

```bash
# 1. Load the Virtual CAN kernel module
sudo modprobe vcan

# 2. Create the virtual network link named vcan0
sudo ip link add dev vcan0 type vcan

# 3. Bring the interface UP
sudo ip link set up vcan0

# 4. Verify that vcan0 is active
ip link show vcan0
```

### Step 2: Start `can_decoder` on `vcan0`

Open your first terminal and start the decoder listening on `vcan0`:

```bash
./can_decoder -i vcan0 -d sample.dbc -o can_log.csv
```

### Step 3: Inject CAN Frames using `can-utils`

Open a second terminal and transmit standard CAN frames using `cansend`:

#### Example 1: Engine Data (`0x100` / 256)
- Engine RPM = 2500 rpm (`raw = 10000 = 0x2710`) -> Bytes `10 27`
- Vehicle Speed = 80.50 km/h (`raw = 8050 = 0x1F72`) -> Bytes `72 1F`
- Throttle Position = 50.0 % (`raw = 127 = 0x7F`) -> Byte `7F`
- Engine Oil Temp = 90.0 °C (`raw = 130 = 0x82`) -> Byte `82`

```bash
cansend vcan0 100#1027721F7F820000
```

#### Example 2: Engine Status (`0x101` / 257)
- Coolant Temp = 88 °C (`raw = 88 - (-40) = 128 = 0x80`) -> Byte `80`
- Fuel Pressure = 380 kPa (`raw = 380 / 4 = 95 = 0x5F`) -> Byte `5F`
- Engine Running & MIL Off -> Byte `01`
- Battery Voltage = 14.1 V (`raw = 14.1 / 0.1 = 141 = 0x8D`) -> Byte `8D`

```bash
cansend vcan0 101#805F018D00000000
```

#### Example 3: Brake System (`0x200` / 512)
- Brake Pedal = 25 % -> Byte `40`
- Brake Pressure = 20.0 bar (`raw = 200 = 0x00C8`) -> Bytes `C8 00`

```bash
cansend vcan0 200#40C800000000
```

#### Example 4: Continuous Simulated Bus Traffic (`cangen`)
To generate random continuous traffic on `vcan0`:

```bash
cangen vcan0 -g 50 -I 100 -L 8
```

---

## Fallback Mock Mode (Zero Hardware)

If you are on a system without CAN drivers or want instant offline testing, run with `--mock`:

```bash
./can_decoder --mock -c 30
```

This starts the dynamic automotive vehicle physics generator, producing 30 frames at 10 Hz, printing the live decoded values and exporting them to `can_log.csv`.

---

## DBC File Format Guide

Vector DBC (`.dbc`) files describe how binary bit streams across CAN frames map to human-readable physical signals.

### Message Syntax (`BO_`)

```dbc
BO_ <MessageID_Dec> <MessageName>: <DLC> <TransmitterNode>
```

- `<MessageID_Dec>`: The CAN ID in **decimal**. (e.g. `256` corresponds to `0x100` hex).
- `<MessageName>`: Identifier string (e.g. `Engine_Data`).
- `<DLC>`: Data Length Code in bytes (typically `1` to `8`).
- `<TransmitterNode>`: Name of the ECU node transmitting this frame (e.g. `Engine_ECU`).

### Signal Syntax (`SG_`)

```dbc
SG_ <SignalName> : <StartBit>|<Length>@<Endian><Sign> (<Scale>,<Offset>) [<Min>|<Max>] "<Unit>" <ReceiverNodes>
```

| Parameter | Meaning | Example |
| :--- | :--- | :--- |
| `<SignalName>` | Unique name of the telemetry channel | `EngineRPM` |
| `<StartBit>` | LSB bit index for Intel; MSB for Motorola | `0` |
| `<Length>` | Bit field width (1 to 64 bits) | `16` |
| `<Endian>` | `1` = Intel (Little-Endian), `0` = Motorola (Big-Endian) | `1` |
| `<Sign>` | `+` = Unsigned integer, `-` = Signed two's complement | `+` |
| `(<Scale>,<Offset>)` | Linear transfer scaling coefficients | `(0.25,0)` |
| `[<Min>|<Max>]` | Minimum and maximum engineering bounds | `[0\|16383.75]` |
| `"<Unit>"` | Unit of measurement string | `"rpm"` |
| `<ReceiverNodes>` | Receiving ECUs listening to this signal | `Dashboard_Display` |

### Mathematical Decoding Formula

When a raw integer $R$ is extracted from the payload bits:

$$\text{Physical Value} = (R \times \text{Scale}) + \text{Offset}$$

*Example*: If raw 16-bit integer $R = 10000$, $\text{Scale} = 0.25$, and $\text{Offset} = 0$:
$$\text{RPM} = (10000 \times 0.25) + 0 = 2500.00\text{ rpm}$$

---

## CLI Options & Usage Reference

```text
Usage: ./can_decoder [OPTIONS]

Options:
  -i, --interface <name>  SocketCAN interface (default: 'vcan0')
  -d, --dbc <path>        Path to Vector DBC database file (default: 'sample.dbc')
  -o, --output <path>     Path to output CSV log file (default: 'can_log.csv')
  -m, --mock              Force mock frame generator (simulated automotive stream)
  -r, --rate <hz>         Mock generator frequency in Hz (default: 10)
  -c, --count <num>       Exit after receiving <num> frames (0 = run forever, default: 0)
  -s, --summary           Display DBC database summary and exit
  -q, --quiet             Quiet mode (suppress live console telemetry tables)
  -h, --help              Display this help message
```

---

## Output Formats

### 1. Live Terminal Visualization

```text
[2026-10-12T17:50:12.450Z] 0x100 (256) Engine_Data        DLC:8 [10 27 72 1F 7F 82 AA 55]
  ├── EngineRPM              :    2500.00 rpm    (raw: 10000 ) [OK]
  ├── VehicleSpeed           :      80.50 km/h   (raw: 8050  ) [OK]
  ├── ThrottlePosition       :      49.80 %      (raw: 127   ) [OK]
  └── EngineOilTemp          :      90.00 degC   (raw: 130   ) [OK]

[2026-10-12T17:50:12.550Z] 0x101 (257) Engine_Status      DLC:8 [80 5F 01 8D 00 00 00 00]
  ├── CoolantTemperature     :      88.00 degC   (raw: 128   ) [OK]
  ├── FuelPressure           :     380.00 kPa    (raw: 95    ) [OK]
  ├── EngineStatus           :       1.00        (raw: 1     ) [OK]
  ├── CheckEngineLight       :       0.00        (raw: 0     ) [OK]
  └── BatteryVoltage         :      14.10 V      (raw: 141   ) [OK]
```

### 2. CSV Telemetry Log Schema (`can_log.csv`)

| Column | Description | Example |
| :--- | :--- | :--- |
| `Timestamp_ISO` | ISO-8601 UTC timestamp | `2026-10-12T17:50:12.450Z` |
| `Timestamp_Sec` | High-resolution monotonic timestamp | `12450.312984` |
| `CAN_ID_Hex` | CAN identifier in hexadecimal | `0x100` |
| `CAN_ID_Dec` | CAN identifier in decimal | `256` |
| `Message_Name` | Message name matched from DBC | `Engine_Data` |
| `DLC` | Payload length in bytes | `8` |
| `Payload_Hex` | Raw payload hexadecimal string | `10 27 72 1F 7F 82 AA 55` |
| `Signal_Name` | Decoded signal name | `EngineRPM` |
| `Raw_Value` | Unpacked integer raw value | `10000` |
| `Physical_Value`| Scaled engineering value | `2500.0000` |
| `Unit` | Engineering measurement unit | `rpm` |

---

## Verification & Unit Testing

Execute the automated test suite to verify math formulas, DBC parsing, and memory safety:

```bash
make test
```

Expected output:
```text
====================================================
 CAN LOGGER & DECODER UNIT TEST SUITE               
====================================================

=== Running DBC Parser Test ===
  [PASS] DBC database loaded successfully
  [PASS] Found at least 4 messages in sample.dbc
  [PASS] Found Engine_Data (0x100)
  [PASS] Message name is Engine_Data
  [PASS] Engine_Data DLC is 8
  [PASS] Engine_Data has 4 signals
  [PASS] Signal 0 name is EngineRPM
  [PASS] EngineRPM start bit is 0
  [PASS] EngineRPM bit length is 16
  [PASS] EngineRPM scale is 0.25
  [PASS] EngineRPM offset is 0.0
  [PASS] EngineRPM unit is 'rpm'
  [PASS] Found Engine_Status (0x101)
  [PASS] CoolantTemperature signal found
  [PASS] Coolant scale is 1.0
  [PASS] Coolant offset is -40.0

=== Running Signal Decoding Math Test ===
  [PASS] DBC loaded for math test
  [PASS] Found Engine_Data definition
  [PASS] Decoded 4 signals
  [PASS] Raw EngineRPM value is exactly 10000
  [PASS] Physical EngineRPM is 2500.0 rpm
  [PASS] EngineRPM is within valid range
  [PASS] Raw VehicleSpeed value is 8050
  [PASS] Physical VehicleSpeed is 80.50 km/h
  [PASS] Raw ThrottlePosition value is 127
  [PASS] Physical ThrottlePosition is ~49.8%
  [PASS] Raw EngineOilTemp value is 130
  [PASS] Physical EngineOilTemp is 90.0 degC

=== Running Mock Bus Acquisition Test ===
  [PASS] Initialized mock bus handle
  [PASS] Bus confirmed in mock mode
  [PASS] Successfully read frame 1 from mock bus
  [PASS] Frame DLC valid (1-8)
  [PASS] Timestamp is positive monotonic time
  [PASS] ISO timestamp generated
  [PASS] Formatted hex payload non-empty

====================================================
 ALL TESTS PASSED SUCCESSFULLY!                    
====================================================
```

---

## License

This project is licensed under the terms of the [MIT License](LICENSE).

<!-- Verification: Local Makefile compilation, test suite, and execution verified cleanly -->
<!-- Release: v1.0.0 Ready for deployment on GitHub -->
