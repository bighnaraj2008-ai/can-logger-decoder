# Automotive Networking & CAN Bus Glossary

`can-logger-decoder` v2.0.0 · Day 3 Automotive Reference

A comprehensive technical glossary of terms, protocols, and acronyms used across `can-logger-decoder` and modern in-vehicle networks.

---

### A
- **ACK (Acknowledge Slot)**: A 2-bit field in CAN frames where the transmitter sends recessive bits and any receiver that received the frame without errors overwrites it with a dominant bit.
- **Arbitration**: The non-destructive bitwise resolution mechanism used when multiple nodes attempt transmission simultaneously. Dominant bits ('0') overwrite recessive bits ('1'), giving the lowest numerical CAN ID bus priority.

### B
- **Bit Stuffing**: The rule where a transmitter inserts a complementary bit after five consecutive identical bits to guarantee clock synchronization edges. Receivers de-stuff automatically.
- **BRP (Baud Rate Prescaler)**: A register setting in CAN controllers (such as MCP2515 `CNF1`) that scales the external crystal clock to derive the Time Quantum ($TQ$).
- **Bus-Off**: An error confinement state entered when the Transmit Error Counter (`TEC`) exceeds 255. In this state, the node's output driver is disabled to prevent bus disruption.

### C
- **CAN 2.0A**: Standard CAN frame format specifying an 11-bit arbitration identifier.
- **CAN 2.0B**: Extended CAN frame format allowing a 29-bit arbitration identifier.
- **CAN FD (Flexible Data-rate)**: An extension to CAN supporting data payloads up to 64 bytes and dual bitrates. (Not supported by classical decoders).
- **CRC (Cyclic Redundancy Check)**: A 15-bit mathematical error-detecting code appended to every CAN frame.

### D
- **DBC (Data Base CAN)**: A Vector-defined ASCII file format specifying CAN messages, signal bit mappings, scaling factors, offsets, and physical engineering units.
- **DLC (Data Length Code)**: A 4-bit field in the CAN control frame encoding the number of data bytes (0–8 for Classical CAN).
- **Dominant Bit**: Logical '0'. Actively driven on the physical differential bus ($V_{DIFF} \approx 2.0\text{ V}$).

### E
- **EFF (Extended Frame Format)**: Frame utilizing a 29-bit identifier marked by flag `CAN_EFF_FLAG (0x80000000)`.
- **Error Active**: Normal operational state where a node actively flags errors on the bus using 6 dominant bits.
- **Error Passive**: Confinement state reached when `TEC` or `REC` reaches 128. Node may only transmit recessive error flags.

### M
- **MCP2515**: Stand-alone SPI CAN controller produced by Microchip Technology.
- **Motorola Endianness**: Big-endian bit ordering where the most significant bit is transmitted first.

### R
- **REC (Receive Error Counter)**: Internal controller counter tracking frame reception errors.
- **Recessive Bit**: Logical '1'. High impedance state ($V_{DIFF} \approx 0.0\text{ V}$).
- **RTR (Remote Transmission Request)**: A frame type requesting another node to transmit data for a given identifier without carrying a payload.

### S
- **SFF (Standard Frame Format)**: Frame utilizing an 11-bit identifier.
- **SocketCAN**: The official Linux kernel CAN subsystem treating CAN interfaces as standard network interfaces (`can0`, `vcan0`).

### T
- **TEC (Transmit Error Counter)**: Internal counter tracking transmission failures and arbitration aborts.
- **Time Quantum ($TQ$)**: The basic discrete unit of time used to construct the bit time in CAN controllers.

### V
- **vcan**: Virtual CAN driver in Linux allowing loopback and network testing without physical hardware.


### F
- **FPS (Frames Per Second)**: Ingestion and decoding throughput metric measuring real-time message handling capacity under synthetic bus saturation.

### Z
- **Zero-Copy Architecture**: Processing technique where CAN payload bytes are referenced or copied directly to final structures without intermediate scratch allocations.