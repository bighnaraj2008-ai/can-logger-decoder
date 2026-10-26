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
