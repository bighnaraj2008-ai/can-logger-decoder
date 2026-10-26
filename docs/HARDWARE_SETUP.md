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

> **INT wiring:** The `/INT` pin is active-low. Connect it to a host GPIO
> capable of external interrupts (edge-triggered, falling edge). The driver
> treats INT as a hint that an RX buffer or error condition is pending, then
> polls `CANINTF` over SPI to determine the exact source. If your host cannot
> spare an interrupt GPIO, the driver also works in polling mode (INT may be
> left unconnected with a pull-up, at the cost of latency).

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

## 4. Interrupt (INT) wiring

The `/INT` pin reflects pending interrupt flags in `CANINTF`:

| CANINTF bit | Meaning | Typical use in this driver |
|:------------|:--------|:---------------------------|
| `RX0IF` | Receive buffer 0 full | Frame available in RXB0 |
| `RX1IF` | Receive buffer 1 full | Frame available in RXB1 |
| `TX0IF`–`TX2IF` | Transmit buffer empty | TX buffer available |
| `ERRIF` | Error condition | Error counters, bus-off |
| `WAKIF` | Wake-up | Bus wake-up |
| `MERRF` | Memory error | RAM/address error |

**Recommended wiring:**

```
/MCP2515 /INT o----|<|----o Host GPIO (external interrupt, falling edge)
                 10 kΩ
                 |
                3.3 V / 5 V  (pull-up to VDD)
```

- Use a **10 kΩ pull-up** to VDD if the module does not already provide one.
- Configure the host GPIO as **input with pull-up**, edge-triggered on the
  **falling edge**.
- In the ISR, set a flag; the main loop calls
  `mcp2515_service_interrupts()` (see `src/hardware/mcp2515_interrupt.c`)
  to read `CANINTF`, classify RX/TX/error, and clear the serviced flags.

If INT is not wired, poll `CANINTF` periodically (e.g. every 1–10 ms) —
acceptable for low-to-moderate bus loads.

---

## 5. Power requirements

| Item | Specification |
|:-----|:--------------|
| Supply voltage (`VDD`) | 4.5 V – 5.5 V (MCP2515); many breakout boards regulate to 3.3 V |
| Operating current | ~5 mA typical (controller only), up to ~15 mA with transceiver |
| CAN transceiver | MCP2551 typically paired; powered from same VDD |
| Decoupling | 0.1 µF ceramic cap as close as possible to VDD/VSS; optional 10 µF electrolytic |
| `/RESET` | Tie high via 10 kΩ pull-up, or drive with a host GPIO after power stable |
| `CLKEN` | Tie high to enable `CLKOUT` (optional host clock source) |

**Power sequencing:** Bring VDD up before or together with the CAN bus
termination. Assert `/RESET` low until VDD is stable, then release high
before the first SPI transaction. The driver begins with a software reset
(`mcp2515_reset()`) regardless of the power-on state.

---

## 6. Crystal / oscillator selection

The MCP2515 derives its CAN bit timing from an external crystal connected to
pins 14/15 (`OSC1`/`OSC2` on the silicon; on breakout boards the crystal is
populated on the module).

| Crystal | Common use | Driver impact |
|:--------|:-----------|:--------------|
| **8.000 MHz** | Hobbyist breakout boards (e.g. NiRen) | `MCP2515_DEFAULT_OSC_HZ` must match; bitrate tables assume the fitted crystal |
| **16.000 MHz** | Industrial shields | Same driver; different CNF1/CNF2/CNF3 values for identical bitrate |

**Important:** A crystal frequency mismatch between the module and the
driver's bit-timing calculation produces a baud-rate error (often 2×), which
causes immediate bit errors and bus-off. Confirm the module's crystal before
calling `mcp2515_set_bitrate()`. See `src/hardware/mcp2515_init.c` and
`include/mcp2515.h` (`MCP2515_DEFAULT_OSC_HZ`).

---

## 7. CAN bus termination

High-speed CAN (ISO 11898-2) requires termination at each end of the bus.

| Termination | Value | Where |
|:------------|:------|:------|
| End-node termination | 120 Ω | Between CANH and CANL at each of the two outermost nodes |
| Measured `CANH`–`CANL` (power off) | ~60 Ω | Two 120 Ω resistors in parallel (healthy bus) |
| Mid-bus nodes | None | Do not terminate intermediate nodes |

Many MCP2515 breakout boards include an on-board 120 Ω terminator with a
solder jumper. Enable it only if this node is one of the two bus endpoints.

---

## 8. Typical wiring diagram

```
 +---------------------------------------------------------------------+
 |                         HOST CONTROLLER                             |
 |                                                                     |
 |  GPIO CS  o----[CS]                                                   |
 |  GPIO MISO o----[SO / MISO]                                           |
 |  GPIO SCK  o----[SCK]                                                 |
 |  GPIO MOSI o----[SI / MOSI]                                           |
 |  GPIO INT o---|<|----[/INT]          10k pull-up                     |
 |                  |                                                    |
 |  +3.3/5 V o------[VCC]                                                |
 |  GND      o------[GND]                                                |
 |                                                                     |
 |  MCP2515 STAND-ALONE CAN CONTROLLER (SPI SLAVE)                     |
 |                                                                     |
 |  [CANH] o-------\                                                     |
 |  [CANL] o--------\______ 120 Ω terminator (if bus endpoint)         |
 |                                |                                    |
 |                         CAN_H / CAN_L                              |
 |                                |                                    |
 |                    [120 Ω] (other end of bus)                       |
 |                                |                                    |
 |                    OTHER CAN NODE(S)                                |
 +---------------------------------------------------------------------+
```

---

## 9. Software configuration sequence (MCP2515)

The driver initializes the MCP2515 in `mcp2515_hw_init()` (`src/hardware/mcp2515_init.c`). The controller must be in **Configuration mode** before bit-timing or filter registers are programmed.

```text
 1. mcp2515_reset()            -> Software reset command (0xC0)
 2. mcp2515_set_mode(CONFIG)   -> Poll CANSTAT until CONFIG mode acknowledged
 3. mcp2515_set_bitrate(Hz)    -> Program CNF1, CNF2, CNF3 (BRP, SJW, PRSEG,
                                    PHSEG1, PHSEG2, SAM)
 4. Write RXB0CTRL / RXB1CTRL  -> Receive buffer control (rollover, format)
 5. Write CANINTE              -> Enable RX0IE, RX1IE, ERRIF interrupts
 6. mcp2515_set_mode(target)   -> Enter NORMAL / LOOPBACK / LISTEN_ONLY
```

### Bit-timing examples (16 MHz crystal)

| Bitrate | BRP | TQ/bit | PRSEG | PHSEG1 | PHSEG2 | SJW | Sample point |
|:--------|:---:|:------:|:-----:|:------:|:------:|:---:|:------------:|
| 125 kbps | 4 | 32 | 6 | 7 | 8 | 1 | ~81% |
| 250 kbps | 2 | 32 | 6 | 7 | 8 | 1 | ~81% |
| 500 kbps | 1 | 32 | 6 | 7 | 8 | 1 | ~81% |
| 1 Mbps   | 1 | 16 | 3 | 4 | 5 | 1 | ~81% |

Program these into `CNF1` (SJW, BRP), `CNF2` (SAM, PHSEG1, PRSEG),
`CNF3` (PHSEG2) via `mcp2515_set_bitrate()`.

### Acceptance filters

With the controller still in CONFIG mode, call
`mcp2515_configure_acceptance_filters()` to program `RXF0`/`RXM0` and
`RXF1`/`RXM1` from a `can_filter_set_t` (see `include/can_filters.h`).
RX buffer 0 uses filter/mask 0; RX buffer 1 uses filter/mask 1.

---

## 10. Troubleshooting quick reference

| Symptom | Likely cause | Check |
|:--------|:-------------|:------|
| `mcp2515_hw_init()` returns `CAN_ERR_IO` / `CAN_ERR_SPI_FAULT` | No SPI response | CS wiring, SPI mode, clock frequency, `/RESET` held low |
| Frames never received | INT not configured / filters blocking | INT edge wiring, CANINTE bits, acceptance filters |
| Immediate bus-off on bus join | Crystal/baud mismatch | Verify module crystal vs driver bit-timing table; check 120 Ω termination |
| Corrupt / shifted data | Wrong SPI mode or bit order | Confirm Mode 0,0 or 1,1, MSB first |
| Interrupt storms | Flags not cleared | `mcp2515_clear_interrupt_flags()` after servicing `CANINTF` |

For MCP2515 SPI register, command-opcode, mode, and bit-timing constants, see
`include/mcp2515.h`. For the state-machine response to SPI faults and bus-off,
see `docs/STATE_MACHINE.md`.