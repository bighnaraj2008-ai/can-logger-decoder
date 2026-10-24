# Hardware SPI & Virtual SocketCAN Troubleshooting Guide

This guide covers step-by-step diagnostic procedures for physical MCP2515 SPI interfaces, virtual SocketCAN (`vcan`) environments, and error recovery diagnostics.

---

## 1. Physical Hardware: Microchip MCP2515 (SPI)

### Symptom: `mcp2515_hw_init()` Fails or Reports `CAN_ERR_IO` / `CAN_ERR_SPI_FAULT`
1. **SPI Mode & Clock Polarity**:
   - The MCP2515 operates strictly in **SPI Mode 0,0** (CPOL=0, CPHA=0) or **SPI Mode 1,1** (CPOL=1, CPHA=1).
   - Verify that your host controller initializes SPI with `MSB First` and clock frequency between 1 MHz and 10 MHz.
2. **Oscillator Frequency Mismatch**:
   - Most hobbyist breakout boards (e.g. NiRen) use an **8.000 MHz** crystal.
   - Industrial shields commonly use a **16.000 MHz** crystal.
   - If the crystal frequency does not match the driver constant, baud rate calculation will be off by a factor of 2, resulting in zero communication and immediate bit-error bus-off.
3. **Hardware Reset Line (`/RESET`)**:
   - Pin 17 (`/RESET`) must be tied high to VDD (via a 10kΩ pull-up) or driven HIGH by a dedicated GPIO. If floating, the chip stays in continuous hardware reset.
4. **Physical Bus Termination**:
   - High-speed CAN requires a nominal **120 Ω** termination resistor at each end of the two outermost nodes.
   - Measure resistance between `CAN_H` and `CAN_L` with power off: it should measure approximately **60 Ω** (two 120 Ω resistors in parallel).

---

## 2. Linux Virtual CAN (`vcan`) Configuration

### Symptom: `vcan_socket_bind()` Reports `CAN_ERR_IO` or "No such device"
Virtual CAN interfaces must be created in the Linux kernel network stack before binding:
```bash
# Load kernel module
sudo modprobe vcan

# Create and bring up vcan0 interface
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Verify interface state
ip link show vcan0
```

### Symptom: `vcan_socket_open()` Returns `CAN_ERR_UNSUPPORTED`
- On Windows or non-Linux host platforms, raw `AF_CAN` sockets are not supported natively by the OS kernel.
- The stack automatically runs in **Mock Bus Simulation Mode** or unit-test software emulation mode without requiring kernel SocketCAN drivers.

---

## 3. Diagnosing Fault-Confinement & Bus-Off States

### Reading TEC and REC Registers
When a node encounters persistent transmission collisions or ACK errors:
1. `TEC >= 96` or `REC >= 96`: Node enters **Warning State** (`ERR_STATE_WARNING`).
2. `TEC >= 128` or `REC >= 128`: Node enters **Error Passive State** (`ERR_STATE_ERROR_PASSIVE`). It may no longer transmit active error flags.
3. `TEC > 255`: Node trips into **Bus-Off** (`ERR_STATE_BUS_OFF`). The output driver is disconnected.

### Automated Recovery Procedure
Our state machine (`src/state/bus_off_recovery.c`) handles recovery according to ISO 11898-1:
- Controller issues a software reset (`mcp2515_reset()`).
- Monitors the bus for 128 occurrences of 11 consecutive recessive bits.
- Restores controller to normal operating mode (`MCP2515_MODE_NORMAL`).
