# Production Deployment & Embedded Flashing Guide

`can-logger-decoder` v2.0.0 · Day 3 Deployment Guide

This guide describes production deployment strategies for `can-logger-decoder` across Linux edge gateways (Raspberry Pi, industrial SBCs) and embedded microcontrollers (STM32, ESP32).

---

## 1. Target Architectures & Toolchains

| Target Platform | Operating System | Compiler / Toolchain | Interface Type |
|:----------------|:-----------------|:---------------------|:---------------|
| Industrial Edge Gateway | Ubuntu Server / Debian ARM64 | `aarch64-linux-gnu-gcc` | SocketCAN (`can0`) |
| Raspberry Pi 4 / Compute Module | Raspberry Pi OS (32/64-bit) | `gcc` native or cross | SPI MCP2515 / SocketCAN |
| STM32F4 / Cortex-M4 | FreeRTOS / Bare-Metal | `arm-none-eabi-gcc` | SPI MCP2515 hardware driver |
| ESP32-WROOM | ESP-IDF / FreeRTOS | `xtensa-esp32-elf-gcc` | SPI MCP2515 / Built-in TWAI |

---

## 2. Linux Edge Gateway Deployment

### 2.1 Systemd Service Configuration

Deploy `can-logger-decoder` as a persistent background daemon managed by `systemd`.

Create `/etc/systemd/system/can-logger.service`:

```ini
[Unit]
Description=Automotive CAN Bus Logger and Real-Time Decoder
After=network.target network-online.target
Wants=network-online.target

[Service]
Type=simple
User=root
Group=root
WorkingDirectory=/opt/can-logger-decoder
ExecStart=/opt/can-logger-decoder/can_decoder -i can0 -dbc /etc/can/sample.dbc -o /var/log/can/telemetry.csv -q
Restart=always
RestartSec=5s

# Process Sandboxing & Hardening
ProtectSystem=full
ProtectHome=true
PrivateTmp=true
LimitNOFILE=65535
Nice=-10

# Standard Output Redirection
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

Enable and start the service:
```bash
sudo systemctl daemon-reload
sudo systemctl enable can-logger.service
sudo systemctl start can-logger.service
sudo systemctl status can-logger.service
```

### 2.2 Persistent SocketCAN Network Configuration

Create `/etc/systemd/network/80-can0.netdev`:
```ini
[NetDev]
Name=can0
Kind=can

[CAN]
BitRate=500000
RestartSec=100ms
```

Or configure via `/etc/network/interfaces`:
```text
auto can0
iface can0 can static
    bitrate 500000
    up ip link set $IFACE txqueuelen 1000
```

### 2.3 Udev Rules for Automated Device Enumeration

Create `/etc/udev/rules.d/80-can-devices.rules`:
```udev
# Auto-configure MCP2515 SPI CAN controllers upon driver binding
SUBSYSTEM=="net", ACTION=="add", ATTRS{interface}=="can*", RUN+="/sbin/ip link set %k type can bitrate 500000 && /sbin/ip link set %k up"
```

---

## 3. Microcontroller Firmware Flashing

### 3.1 STM32 Flashing via OpenOCD / ST-Link

```bash
# Build firmware binary
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O2 -Iinclude src/core/*.c src/hardware/*.c -o build/firmware.elf
arm-none-eabi-objcopy -O binary build/firmware.elf build/firmware.bin

# Flash target via OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "init" \
    -c "reset halt" \
    -c "flash write_image erase build/firmware.bin 0x08000000" \
    -c "reset run" \
    -c "shutdown"
```

### 3.2 ESP32 Flashing via esptool.py

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
    write_flash -z 0x10000 build/can_logger_esp32.bin
```

---

## 4. Production Checklist

- [ ] High-speed 120 Ω bus termination verified at both physical endpoints.
- [ ] Receive buffer sizes configured (`sysctl net.core.rmem_max=1048576`).
- [ ] Log partition `/var/log/can/` mounted on high-endurance media (industrial SLC SD or eMMC).
- [ ] Hardware watchdog timer enabled and kicked in main loop.
