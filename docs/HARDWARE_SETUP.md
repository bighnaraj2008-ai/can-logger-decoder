# Hardware & Wiring Specifications — MCP2515 to Host Controller

`can-logger-decoder` v2.0.0 · Day 3 Hardware Guide

This document describes the physical and electrical integration of the
**Microchip MCP2515** stand-alone CAN controller (SPI) with a host controller
(MCU or SBC) running `can-logger-decoder`. It covers SPI pin mappings, chip
select (CS) configurations, interrupt (INT) wiring, power requirements,
oscillator selection, CAN bus termination, and the software configuration
sequence the driver expects.

---

## 1. MCP2515 Module Pinout (typical breakout board)

Most hobbyist MCP2515 breakout boards (e.g. NiRen / CAN-BUS Shield style)
expose the following headers. Map each to the corresponding host controller
pin.

| Module pin | Function | Direction | Notes |
|:-----------|:---------|:----------|:------|
| `VCC` | Power supply | In | 3.3 V or 5 V (module dependent) |
| `GND` | Ground | In | Common ground with host |
| `CS` / `/CS` | SPI chip select | In | Active-low. Driven by host GPIO. |
| `SO` / `MISO` | SPI data out | Out (MCP2515) | Host MISO input |
| `SCK` | SPI clock | In | Driven by host SPI master |
| `SI` / `MOSI` | SPI data in | In (MCP2515) | Host MOSI output |
| `INT` / `/INT` | Interrupt request | Out (MCP2515) | Active-low, open-drain on many boards |
| `CANH` | CAN high | I/O | Differential CAN bus high |
| `CANL` | CAN low | I/O | Differential CAN bus low |

### MCP2515 silicon pin reference (18-pin DIP/SOIC)

For custom PCB designs, the MCP2515 silicon pins relevant to this driver are:

| Silicon pin | Name | Role in this driver |
|:------------|:-----|:--------------------|
| 1 | `/CS` | SPI chip select (active low) |
| 2 | `SO` | SPI MISO (data out of MCP2515) |
| 3 | `SCK` | SPI serial clock |
| 4 | `SI` | SPI MOSI (data into MCP2515) |
| 5 | `/INT` | Interrupt output (RX0IF, RX1IF, ERRIF, …) |
| 6 | `VDD` | Positive supply |
| 7 | `VSS` | Ground |
| 8 | `CLKOUT` | Divided-down crystal output (optional host clock) |
| 9 | `CLKEN` | CLKOUT enable (tie high to enable) |
| 10 | `/RESET` | Hardware reset (tie high via 10 kΩ or drive with GPIO) |
| 11–13 | `RX0BF`/`RX1BF`/`TX0RTS` | Buffer status (not used by this driver) |
| 14–15 | `TX1RTS`/`TX2RTS` | Transmit request (not used by this driver) |
| 16 | `CANL` | CAN differential low |
| 17 | `CANH` | CAN differential high |
| 18 | `VSS` | Ground |

> **Note:** Pins 11–15 are buffer status/transmit-request pins. This driver
> polls register status over SPI rather than using them, so they may be left
> unconnected on minimal designs.

---

## 2. Recommended SPI pin mappings (host controller examples)

| Host controller | CS | SO/MISO | SCK | SI/MOSI | INT | Notes |
|:----------------|:---:|:-------:|:---:|:-------:|:---:|:------|
| Arduino Uno / Nano / Pro Mini | D10 | D12 | D13 | D11 | D2 | INT on external-interrupt pin |
| Arduino Mega 2560 | D53 | D50 | D52 | D51 | D2 | CS must be hardware SS (D53) |
| ESP32 (any GPIO) | GPIO5 | GPIO19 | GPIO18 | GPIO23 | GPIO4 | Fully flexible; any pins |
| Raspberry Pi (SPI0) | CE0 (GPIO8) | MISO (GPIO9) | SCLK (GPIO11) | MOSI (GPIO10) | GPIO25 | `spidev` or bcm2835 |
| STM32 (SPI1, e.g. F4) | PA4 | PA6 | PA5 | PA7 | PB0 | Alternate-function SPI pins |

---

## 3. Chip Select (CS) line configuration

The MCP2515 is an SPI slave and only responds when `/CS` is asserted low.

| Parameter | Requirement |
|:----------|:------------|
| Polarity | Active low (`/CS`) |
| Idle state | High (de-asserted) between transactions |
| Assertion | Pull low before the first SCK edge; keep low for the full command |
| De-assertion | Pull high after the last SCK edge |
| Multiple devices | Each MCP2515 needs a dedicated CS line; share SCK/MOSI/MISO |
| CS-to-SCK setup | ≥ 100 ns (typical); hold `/CS` low ≥ 1 SCK period before clocking |
| SPI mode | **Mode 0,0** (CPOL=0, CPHA=0) or **Mode 1,1** (CPOL=1, CPHA=1) |
| Bit order | MSB first |
| Max SPI clock | 10 MHz (use 1–8 MHz on breadboards / long wires) |

### CS software handling in this driver

`src/hardware/mcp2515_spi.c` performs one register access per SPI transaction:
1. Assert `/CS`.
2. Send command opcode (e.g. `READ 0x03`, `WRITE 0x02`, `BITMODIFY 0x05`).
3. Send register address, then read/write data byte(s).
4. De-assert `/CS`.

The transport is pluggable via `mcp2515_spi_ops_t` (see `include/mcp2515.h`),
so the same driver works with real SPI, a software register model, or the
mock SPI harness in `tests/harness/mock_spi.*` — no physical CS line required
during host-based unit tests.

---

## 4. Power Supply & Logic Level Shifting Requirements

Proper electrical power delivery and signal level matching are critical to ensure error-free SPI transactions and CAN transceiver operation.

### 4.1 Operating Voltage Specifications

| Component | Minimum | Typical | Maximum | Notes |
|:----------|:--------|:--------|:--------|:------|
| `MCP2515 VDD` | 2.7 V | 5.0 V | 5.5 V | Max SPI clock is limited to 5 MHz below 4.5 V |
| `MCP2551 VDD` | 4.5 V | 5.0 V | 5.5 V | Standard high-speed transceiver requires 5.0 V |
| `TJA1050 VDD` | 4.75 V | 5.0 V | 5.25 V | Alternative 5 V transceiver |
| `SN65HVD230` | 3.0 V | 3.3 V | 3.6 V | 3.3 V native transceiver |

### 4.2 Logic Level Shifting (3.3 V Host to 5 V MCP2515)

When pairing a 5 V MCP2515 module with a 3.3 V host MCU (e.g., ESP32, STM32, Raspberry Pi):
- **MCP2515 Inputs (`SCK`, `SI/MOSI`, `/CS`)**: While MCP2515 inputs recognize 3.3 V signals as logical HIGH (VIH min is $0.7 \times VDD = 3.5\text{ V}$, but typically works at 3.3 V), bidirectional or unidirectional logic level shifters (e.g., 74LVC245 or BSS138 FET-based converters) are strongly recommended for industrial stability.
- **MCP2515 Output (`SO/MISO`)**: MCP2515 outputs 5 V on `SO` when powered from 5 V. Driving a 5 V `SO` into a non-5V-tolerant 3.3 V GPIO will damage the host controller. Use a fast logic level shifter or a resistive voltage divider ($1\text{ k}\Omega / 2\text{ k}\Omega$).
- **Dual-Voltage Modules**: Many modern breakout boards operate the MCP2515 at 3.3 V logic while powering the transceiver at 5.0 V. Verify your module schematic.

### 4.3 Decoupling & Power Sequencing

- Place a **0.1 µF (100 nF) ceramic decoupling capacitor** directly across `VDD` and `VSS` (pins 6 and 7 of MCP2515) within 5 mm of the IC pins.
- Place a **10 µF tantalum or low-ESR electrolytic capacitor** across the module power rail.
- Ensure power sequencing stabilizes `VDD` before releasing `/RESET` from ground.

---

## 5. Crystal / Oscillator Selection & Bit-Timing Calculations

The MCP2515 derives all internal bit-timing time quanta ($TQ$) from an external crystal oscillator connected across `OSC1` and `OSC2`.

| Parameter | 8.000 MHz Crystal | 16.000 MHz Crystal |
|:----------|:------------------|:-------------------|
| Common modules | NiRen, standard blue boards | Industrial Arduino CAN shields |
| `F_OSC` | 8,000,000 Hz | 16,000,000 Hz |
| `T_OSC` | 125 ns | 62.5 ns |
| Minimum $TQ$ ($BRP=0$) | $2 \times T_{OSC} = 250\text{ ns}$ | $2 \times T_{OSC} = 125\text{ ns}$ |
| Max CAN Bitrate | 1 Mbps (at 8 TQ/bit) | 1 Mbps (at 16 TQ/bit) |
| Configuration Header | `#define MCP2515_DEFAULT_OSC_HZ 8000000UL` | `#define MCP2515_DEFAULT_OSC_HZ 16000000UL` |

> **Critical Warning:** Setting `mcp2515_set_bitrate()` with an oscillator frequency assumption that does not match the fitted crystal will cause an immediate $2\times$ baud mismatch, resulting in transmission error counter (`TEC`) spikes and immediate transition to `BUS_OFF`.
