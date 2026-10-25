# Changelog

All notable changes to the `can-logger-decoder` project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.1.0] - 2026-10-25

### Day 2 Test Coverage & Fixture Validation

#### Added
- **Automated Test Fixture Catalog (`tests/fixtures/`)**:
  - `tests/fixtures/raw_frames.bin`: 8 raw self-delimiting CAN wire frames (standard, extended, RTR, error).
  - `tests/fixtures/corrupted_frames.hex`: 11 hex anomaly frames (DLC out-of-bounds, truncated header, CRC corruption).
  - `tests/fixtures/extended_frames.bin`: 5 extended 29-bit identifier binary frames (SAE J1939 & UDS diagnostics).
  - `tests/fixtures/bus_off_sequence.hex`: 10 error escalation vectors triggering ISO 11898-1 bus-off transitions.
- **Edge-Case & Boundary Unit Test Suites (`tests/edge_cases/`)**:
  - `tests/edge_cases/test_dlc_mismatch.c`: Classical CAN DLC bounds (0..8), CAN FD mapping, and wire truncation.
  - `tests/edge_cases/test_invalid_arbitration.c`: 11-bit and 29-bit identifier validation and flag cleansing.
  - `tests/edge_cases/test_buffer_overflow.c`: Socket and RX ring buffer congestion handling and recovery.
  - `tests/edge_cases/test_bus_off_trigger.c`: TEC/REC counter thresholds, bus-off trigger, and recovery controller.
- **Test Harness Drivers & Mock Subsystems (`tests/harness/`)**:
  - `tests/harness/mock_spi.c` & `mock_spi.h`: Mock SPI communication driver for MCP2515 hardware emulation.
  - `tests/harness/mock_vcan.c` & `mock_vcan.h`: Mock virtual SocketCAN packet generator and burst simulator.
  - `tests/harness/assertions.c`: Custom test assertion library with colorized diagnostics and pass rate tracking.
- **Integration, Performance & Sanitizer Harnesses**:
  - `tests/performance/test_parser_perf.c`: High-throughput parser benchmark measuring frames/sec and latency.
  - `tests/stress/test_can_storm.c`: High-frequency CAN storm stress test (50,000 frames) asserting 100% fidelity.
  - `tests/memory/test_leak_sanitizer.c`: Valgrind / AddressSanitizer heap memory leak safety harness.
  - `tests/integration/test_mcp2515_loopback.c`: MCP2515 hardware loopback integration test.
  - `tests/integration/test_vcan_bridge.c`: Physical SPI to virtual socket bridge routing test.
  - `tests/schema_metadata_tests.c`: Protocol constants and schema metadata validation check.
  - `src/utils/test_logger.c`: Isolated memory-backed logging subsystem for test runs.
  - `include/test_config.h`: Centralized test configuration constants and timeouts.
- **Automation Scripts & Continuous Integration**:
  - `scripts/run_unit_tests.sh`: Day 2 enhanced automated test runner with assertion parsing and exit codes.
  - `scripts/generate_test_report.sh`: Markdown test execution report generator script.
  - `scripts/run_valgrind.sh`: Automated Valgrind memory leak checking script.
  - `scripts/validate_fixtures.sh`: Automated fixture checksum validation script.
  - `scripts/clean_test_artifacts.sh`: Test artifact and compiled binary cleaner script.
  - `.github/workflows/test_coverage.yml`: GitHub Actions workflow for test coverage and memory sanitizers.
- **Documentation & Examples**:
  - `docs/TEST_COVERAGE.md`: Comprehensive test coverage specifications, fixture catalog, and edge-case matrix.
  - `docs/EDGE_CASES.md`: Failure modes, ISO standards, and recovery playbooks.
  - `docs/FIXTURE_GUIDE.md`: Guide on authoring custom binary and hex test fixtures.
  - `docs/BENCHMARK_TEMPLATE.md`: Telemetry performance benchmark reporting template.
  - `examples/custom_fixture_generator.py`: Python utility script to generate custom test fixtures.

## [2.0.0] - 2026-10-24

### Day 1 Architectural Refactoring

#### Added
- **Core Layer Modularization**:
  - `include/can_core.h`: Modular core CAN data structures, identifier bitmasks, and DLC constants.
  - `src/core/can_parser.c`: Modular raw CAN frame payload parsing logic and mock telemetry generator.
  - `src/core/can_frame.c`: Low-level wire frame serialization, deserialization, and CRC-8 payload integrity routines.
  - `src/core/dlc_validator.c`: Strict ISO 11898-1 and ISO 15765-2 Data Length Code (DLC) bounds validation.
  - `src/core/can_router.c`: Bidirectional frame routing module between physical SPI hardware and virtual socket interfaces.
- **Hardware Abstraction Layer (MCP2515)**:
  - `include/mcp2515.h`: Full register map, SPI command opcodes, and bit-timing calculation parameters.
  - `src/hardware/mcp2515_spi.c`: Pluggable SPI transport operations and software register fallback cache.
  - `src/hardware/mcp2515_init.c`: Modular controller reset and hardware initialization state sequences.
  - `src/hardware/mcp2515_interrupt.c`: Refined interrupt service routine dispatcher for SPI events.
  - `src/hardware/mcp2515_filters.c`: Hardware acceptance filter and mask register setup for RXF0-RXF5 / RXM0-RXM1.
- **Virtual Network Layer (SocketCAN / vcan)**:
  - `include/vcan_interface.h`: Virtual socket interface definitions, polling signatures, and frame routing APIs.
  - `src/network/vcan_socket.c`: Virtual socket opening, binding, non-blocking configuration, and error descriptions.
  - `src/network/vcan_listener.c`: Efficient frame read/write routines and blocking/non-blocking polling loops.
- **State Machine & Fault Confinement**:
  - `src/state/error_machine.c`: Fault-confinement state machine managing Warning, Error Passive, and Bus-Off states.
  - `src/state/bus_off_recovery.c`: ISO 11898-1 automatic bus-off recovery sequence controller.
- **Utilities & Telemetry**:
  - `include/error_codes.h`: Canonical centralized `can_error_t` enumeration and inline descriptions.
  - `src/utils/logger.c`: Structured logging framework supporting multiple verbosity levels and CSV logging.
  - `src/utils/crc_check.c`: 15-bit ISO CAN CRC calculation and frame integrity verification routines.
  - `src/utils/metrics.c`: Runtime telemetry, throughput (fps/kbps) calculation, and frame drop counter tracking.
- **Comprehensive Unit Test Suites**:
  - `tests/test_parser.c`: Core parser, wire format, and frame validation test suite.
  - `tests/test_state_machine.c`: Error recovery state transitions and bus-off recovery test suite.
  - `tests/test_vcan.c`: Virtual socket interface lifecycle and frame transmission test suite.
  - `tests/test_filters.c`: MCP2515 hardware filter mask and identifier matching test suite.
  - `tests/test_metrics.c`: Runtime telemetry and frame drop counter verification test suite.
  - `tests/test_router.c`: Bidirectional frame routing and drop verification test suite.
- **Build & CI Automation**:
  - `Makefile`: Modular GNU makefile with clean targets for Linux and Windows.
  - `CMakeLists.txt`: Cross-platform CMake 3.10+ build configuration.
  - `.github/workflows/ci.yml`: Matrix automated CI builds with GCC and Clang.
  - `.clang-format`: Strict LLVM-based code formatting configuration.
  - `scripts/run_unit_tests.sh`: Automated test runner shell script.
  - `scripts/benchmark_decoder.sh`: Frame throughput and latency benchmarking script.
  - `scripts/smoke_test.sh`: Pre-flight integration smoke test script.
- **Technical Documentation**:
  - `docs/ARCHITECTURE_REFACTOR.md`: Detailed Day 1 architectural design specifications.
  - `docs/STATE_MACHINE.md`: Reference guide for fault-confinement states and recovery flow.
  - `docs/PERFORMANCE_TUNING.md`: High-speed processing and buffer optimization guide.
  - `docs/TROUBLESHOOTING.md`: Physical SPI and SocketCAN diagnostic troubleshooting guide.

#### Changed
- Redesigned repository layout from monolithic flat structure into layered enterprise architecture.
- Centralized error codes across all modules to ensure zero undefined return states.
