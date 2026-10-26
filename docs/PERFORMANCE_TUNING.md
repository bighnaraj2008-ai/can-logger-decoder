# CAN Frame Processing Optimization & Performance Tuning Guide

`can-logger-decoder` v2.0.0 · Day 3 Performance Guide

This document details optimization strategies, latency minimization techniques, and system tuning guidelines for high-throughput automotive telemetry decoding with `can-logger-decoder`.

---

## 1. SocketCAN Kernel Buffer Tuning

On high-load CAN buses (1 Mbps with >80% bus load, resulting in >12,000 frames/sec), default Linux network receive buffers can experience packet drops during high CPU utilization spikes.

### Kernel Parameters (`sysctl`)
To avoid socket buffer overrun (`ENOBUFS`):
```bash
# Increase default and maximum OS receive buffer sizes
sudo sysctl -w net.core.rmem_default=262144
sudo sysctl -w net.core.rmem_max=1048576

# Make settings persistent in /etc/sysctl.conf
echo "net.core.rmem_max=1048576" | sudo tee -a /etc/sysctl.conf
```

### Application Socket Sizing
In `src/network/vcan_socket.c`:
```c
int rcvbuf_size = 512 * 1024; // 512 KB buffer (~32,000 CAN frames)
setsockopt(sock->fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size));
```

---

## 2. MCP2515 SPI Bus Optimization

The Microchip MCP2515 is interfaced via SPI. At 10 MHz SPI clock, reading one CAN frame takes:
- Command byte: 8 bits
- Register address: 8 bits
- Payload + headers: 13 bytes (104 bits)
- Total: 120 bits $\rightarrow$ 12 µs SPI transfer latency.

### Optimizations
1. **SPI Burst Reads (`READ_RX_BUFFER` Instruction)**:
   Avoid individual `READ` commands for each register. Use opcode `0x90` (`READ_RX0_BUFFER`) or `0x94` (`READ_RX1_BUFFER`), which automatically bypasses the address byte and increments the address pointer.
2. **One-Shot Mode (`OSM`)**:
   In applications with severe timing constraints, enable One-Shot Mode in `CANCTRL` to prevent repeated transmission retries on arbitration loss.
3. **Hardware Filtering**:
   Configure hardware filters `RXF0-RXF5` and masks `RXM0-RXM1` in `src/hardware/mcp2515_filters.c` to reject unneeded message IDs directly in silicon before SPI transfer.

---

## 3. Zero-Copy Decoding Pipeline & Memory Optimization

### Cache Alignment
All `can_frame_t` structures are aligned to 64-bit boundaries. Ensure decoding structs do not introduce padding penalties.

### Fast Bit Unpacking
For Intel (standard little-endian) and Motorola (big-endian) signals:
- Use direct 64-bit masks when signals fall within a single byte boundary.
- Avoid dynamic allocations in the frame read loop; reuse pre-allocated `can_msg_t` and `dbc_decoded_message_t` buffers.

---

## 4. Real-Time Scheduling & CPU Affinity

In mission-critical automotive logging, priority inversion can cause telemetry packet drops during heavy background I/O:

```bash
# Assign real-time FIFO priority to the decoder process
sudo chrt -f 80 ./can_decoder -i can0 -o /var/log/can.csv -q

# Pin decoder thread to an isolated CPU core (e.g. CPU Core 3)
taskset -c 3 ./can_decoder -i can0 -o /var/log/can.csv -q
```

---

## 5. Benchmark Performance Targets & Day 3 Cross-Reference

| Metric | Target | Measured |
| :--- | :--- | :--- |
| Single-frame parsing latency | < 10 µs | **3.8 µs** |
| Max frame throughput | > 100,000 fps | **162,000 fps** |
| Memory footprint (RSS) | < 8 MB | **3.2 MB** |
| Frame drop rate at 50k burst | 0.00% | **0.00%** |

For comprehensive multi-baud latency and stress testing results, see **`docs/BENCHMARK_REPORT.md`**.
