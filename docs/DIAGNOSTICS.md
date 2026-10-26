# Hardware Diagnostic & Bus Monitoring Playbook

`can-logger-decoder` v2.0.0 · Day 3 Diagnostic Procedures

This operational playbook defines standard testing procedures, physical layer diagnostics, and signal verification methodologies for troubleshooting CAN networks and MCP2515 SPI interfaces.

---

## 1. Physical Layer Multimeter Testing (Power Off)

Always power down the CAN cluster before measuring passive termination resistance.

| Test Point | Expected Resistance | Diagnostic Interpretation |
|:-----------|:--------------------|:--------------------------|
| `CAN_H` to `CAN_L` | **58 Ω – 62 Ω** | Healthy bus; both 120 Ω endpoint termination resistors present in parallel. |
| `CAN_H` to `CAN_L` | **118 Ω – 122 Ω** | Missing one termination resistor; reflections and frame corruption likely at >250 kbps. |
| `CAN_H` to `CAN_L` | **> 1000 Ω (Open)** | Broken bus wire or missing termination entirely; severe ringing. |
| `CAN_H` to `CAN_L` | **0 Ω – 5 Ω (Short)** | Short circuit between differential lines; bus inoperative. |
| `CAN_H` to `GND` | **> 100 kΩ** | Normal isolation; no ground short. |
| `CAN_L` to `GND` | **> 100 kΩ** | Normal isolation; no ground short. |

---

## 2. Oscilloscope Signal Probing (Power On & Active Bus)

Connect oscilloscope probe channel 1 to `CAN_H` and channel 2 to `CAN_L` referenced to common ground. Compute math channel `CH1 - CH2` (Differential Voltage $V_{DIFF}$).

```
 Dominant State (Logical '0'):
   CAN_H = +3.5 V
   CAN_L = +1.5 V
   V_DIFF = CAN_H - CAN_L = +2.0 V (> 0.9 V threshold)

 Recessive State (Logical '1'):
   CAN_H = +2.5 V
   CAN_L = +2.5 V
   V_DIFF = CAN_H - CAN_L = 0.0 V (< 0.5 V threshold)
```

### Signal Fault Waveforms
- **Severe Ringing / Overshoot**: Indicates impedance mismatch or missing 120 Ω termination.
- **Slow Rise/Fall Times**: Excessive capacitive bus loading (too many stubs, excessive cable length).
- **Common Mode DC Shift**: Ground potential difference between isolated ECUs; ensure common ground return.

---

## 3. Logic Analyzer SPI Capture Verification

When debugging communication failures between the host controller and MCP2515:
1. Verify `/CS` drops low at least 100 ns prior to the first SCK rising edge.
2. Verify SPI Mode:
   - **Mode 0,0**: SCK idles LOW; data sampled on leading RISING edge.
   - **Mode 1,1**: SCK idles HIGH; data sampled on trailing RISING edge.
3. Check `mcp2515_reset()`: Host transmits `0xC0`; controller acknowledges by setting `CANSTAT = 0x80` (CONFIG mode).

---

## 4. Linux SocketCAN Diagnostics Suite

### 4.1 Bus Statistics & Error Counters
```bash
ip -details -statistics link show can0
```
Key outputs to inspect:
- `RX errors`, `TX errors`: Non-zero indicates physical layer noise or baud mismatch.
- `state`: Must report `ERROR-ACTIVE`. If `ERROR-PASSIVE` or `BUS-OFF`, follow `docs/INCIDENT_PLAYBOOK.md`.

### 4.2 Raw Packet Sniffing & Logging
```bash
# Colorized sniffer with delta timing
cansniffer -c can0

# Timestamped raw frame dump
candump -tz -cae can0
```

### 4.3 Diagnostic Frame Injection
```bash
# Inject single standard frame (ID 0x100, payload 8 bytes)
cansend can0 100#1027721F7F82AA55

# Generate continuous test stream at 100 frames/sec
cangen can0 -g 10 -v
```

---

## 5. Bus Load & Error Rate Calculation

### 5.1 Bus Load Formula
$$\text{Bus Load } (\%) = \frac{\sum (\text{Frame Bits} \times \text{Frame Rate})}{\text{Baud Rate (bps)}} \times 100$$
A standard frame with 8 data bytes contains approx 111 to 128 bits including stuff bits.
- Target load: $< 60\%$ continuous
- Saturation threshold: $> 80\%$ (queue delays increase exponentially)
