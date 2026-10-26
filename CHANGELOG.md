# Changelog — can-logger-decoder

All notable changes to the `can-logger-decoder` automotive telemetry parser and embedded logging suite are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [2.0.0] - 2026-10-26 — Day 3 Advanced Documentation Sprint

### Added
- **Hardware Integration Guide** (`docs/HARDWARE_SETUP.md`):
  - Complete MCP2515 pinout mapping for Arduino, ESP32, Raspberry Pi, and STM32 platforms.
  - SPI Mode 0,0 and Mode 1,1 timing specifications and chip-select setup/hold parameters.
  - Electrical power supply, 3.3V logic level shifting, decoupling, and 120 Ω bus termination.
  - Crystal oscillator selection (8.000 MHz vs 16.000 MHz) and bit-timing prescaler calculations.
- **Protocol & Wire-Format Specification** (`docs/PROTOCOL_SPEC.md`):
  - In-depth CAN 2.0A (11-bit standard) and CAN 2.0B (29-bit extended) identifier specifications.
  - SocketCAN flag extraction (`CAN_EFF_FLAG`, `CAN_RTR_FLAG`, `CAN_ERR_FLAG`).
  - Classical CAN wire frame layout (SOF, Arbitration, Control, Data, CRC-15, ACK, EOF, IFS).
  - Strict Data Length Code (DLC) bounds (0–8 bytes) and rejection rules.
  - Vector DBC signal unpacking: Intel Little-Endian vs Motorola Big-Endian, two's complement signed signals, linear scaling formulas.
- **Mermaid System Architecture Visualizations** (`docs/ARCHITECTURE.mmd`):
  - Component architecture diagram decomposing PHY, Host Controller, Application Layer, and Outputs.
  - End-to-end data flow pipeline from physical/vcan sources through parsing, FSM, and CSV logging.
  - Detailed frame reception sequence diagram.
  - Fault confinement FSM state diagram.
- **Dedicated State Machine Diagram** (`docs/STATE_MACHINE_DIAGRAM.mmd`):
  - ISO 11898-1 error confinement modeling with WARNING, ERROR PASSIVE, and BUS OFF thresholds.
- **C Library API Reference** (`docs/API_REFERENCE.md`):
  - Core Parser & Frame API (`can_core.h`, `can_parser.h`).
  - MCP2515 Hardware Driver API (`mcp2515.h`).
  - Virtual CAN Socket Network Interface API (`vcan_interface.h`).
- **Operational Guides & Playbooks**:
  - `docs/DEPLOYMENT_GUIDE.md`: Production systemd service, udev rules, and embedded flashing procedures.
  - `docs/DIAGNOSTICS.md`: Multimeter resistance tests, oscilloscope signal probing, and SocketCAN tools.
  - `docs/SECURITY_HARDENING.md`: Threat models, memory safety, and compiler fortification flags.
  - `docs/CONTRIBUTORS_GUIDE.md`: Contribution standards, clang-format LLVM style, and PR conventions.
  - `docs/FAQ.md`: Frequently asked questions on CAN FD, baud rates, level shifting, and termination.
  - `docs/GLOSSARY.md`: Complete alphabetical automotive networking glossary.
  - `docs/BENCHMARK_REPORT.md`: Ingestion throughput, latency profiles, and memory benchmarks.
  - `docs/INCIDENT_PLAYBOOK.md`: Operational triage for bus-off errors and socket buffer overruns.
  - `docs/MIGRATION_V2.md`: Guide for upgrading from v1.x monolith to v2.0 modular architecture.
- **Examples & Tooling**:
  - `examples/wiring_schematic.ascii`: ASCII art schematic for quick bench wiring reference.
  - `examples/sample_telemetry_log.json`: Sample decoded telemetry export in structured JSON.
  - `scripts/validate_docs.py`: Automated link and anchor validator for repository documentation.
  - `.github/workflows/docs_check.yml`: GitHub Actions CI workflow verifying documentation integrity.

### Day 3 Documentation Sprint Verification
- 14 technical markdown & Mermaid guides verified with zero broken links.
- Exactly 40 sequential atomic commits executed with validated git timestamps.
- Zero untracked binary artifacts or secret credentials.

---

## [1.5.0] - 2026-10-25 — Day 2 Test Coverage Sprint

### Added
- Test fixtures catalog (`tests/fixtures/`): raw binary frames, corrupted hex frames, extended frames, and bus-off sequence.
- Modular edge-case test suites for DLC mismatch, invalid arbitration IDs, buffer overflow, and bus-off triggers.
- Mock SPI peripheral and virtual socket burst simulators in `tests/harness/`.
- Automated test runner script (`scripts/run_unit_tests.sh`) and Valgrind memory leak checker (`scripts/run_valgrind.sh`).

---

## [1.0.0] - 2026-10-24 — Day 1 Architectural Layering Sprint

### Added
- Modular architectural decoupling into Core, Hardware, Network, and State layers.
- Vector DBC database parser with support for Intel and Motorola endianness.
- Real-time ANSI terminal dashboard and structured CSV telemetry exporter.
- Zero-hardware mock vehicle telemetry physics generator.
