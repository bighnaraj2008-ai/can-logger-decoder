# CAN Bus Logger & Signal Decoder (`can-logger-decoder`)

[![CI Build](https://img.shields.io/badge/CI-Passing-brightgreen?style=flat-square&logo=githubactions)](.github/workflows/ci_build.yml)
[![Static Analysis](https://img.shields.io/badge/Static%20Analysis-cppcheck%20%7C%20clang--tidy-blue?style=flat-square)](.github/workflows/static_analysis.yml)
[![C Standard](https://img.shields.io/badge/Language-C99%20%2F%20GNU99-blue?style=flat-square&logo=c)](https://en.wikipedia.org/wiki/C99)
[![Code Style](https://img.shields.io/badge/Code%20Style-LLVM%20clang--format-blueviolet?style=flat-square)](.clang-format)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](LICENSE)

A high-performance, robust command-line CAN (Controller Area Network) bus logger and real-time signal decoder written in portable C (GNU99).

Designed for automotive engineers, embedded systems developers, and telemetry analysts, `can-logger-decoder` reads raw CAN frames directly from Linux SocketCAN interfaces (such as physical `can0` or virtual `vcan0`), parses industry-standard Vector `.dbc` databases, decodes bit-packed signals into scaled engineering units, displays a colorized live console dashboard, and logs data to structured CSV files.

When physical hardware or virtual network interfaces are not available (e.g. running outside Linux or on development workstations), the engine automatically falls back to an internal physics-based vehicle telemetry simulator.

---

## System Architecture

`can-logger-decoder` is layered into **Core**, **Hardware** (MCP2515 SPI), **Network** (SocketCAN/vcan), **State** (error recovery / bus-off), and **Application** (DBC decoder, dashboard, CSV logger) layers.

Full Mermaid component, data-flow, sequence, and state-machine diagrams live in:

- **`docs/ARCHITECTURE.mmd`** Ã¢â‚¬â€ System component architecture diagram
- **`docs/STATE_MACHINE_DIAGRAM.mmd`** Ã¢â‚¬â€ Fault confinement FSM state diagram

### Architecture Overview

```
                        +---------------------------+
                        |     Application Layer     |
                        |  CLI Ã¢â‚¬Â¢ Dashboard Ã¢â‚¬Â¢ CSV    |
                        |  DBC Decoder Ã¢â‚¬Â¢ Runtime    |
                        |     Metrics & Logging     |
                        +-------------+-------------+
                                      |
                                      v
                        +---------------------------+
                        |      Network Layer        |
                        |  SocketCAN / Virtual CAN  |
                        |  Non-blocking I/O, Poll   |
                        |  Loop, Frame Routing      |
                        +-------------+-------------+
                                      |
                                      v
                        +---------------------------+
                        |     Hardware Layer        |
                        |  MCP2515 SPI Driver       |
                        |  Init, Filters, Interrupt |
                        +-------------+-------------+
                                      |
                                      v
                        +---------------------------+
                        |      State Layer          |
                        |  Error Confinement FSM    |
                        |  Bus-Off Recovery         |
                        +-------------+-------------+
                                      |
                                      v
                        +---------------------------+
                        |       Core Layer          |
                        |  CAN IDs, DLC, Wire/Fram  |
                        |  CRC-8, Error Codes       |
                        +-------------+-------------+
                                      |
          +---------------------------+---------------------------+
          |                                                       |
          v                                                       v
[ Linux SocketCAN ]                                       [ Mock Telemetry ]
(Physical: can0, can1)                                 (Vehicle physics sim)
(Virtual:  vcan0, vcan1)                                 (High-fidelity bit packing)
```

### Layer Responsibilities

| Layer | Responsibility |
|-------|----------------|
| Core | CAN identifiers, DLC limits, frame wire serialization, CRC-8 checksums, canonical error codes |
| Bus Acquisition | SocketCAN binding with automatic mock vehicle physics generator |
| Hardware | MCP2515 SPI register map, init sequence, bit-timing, acceptance filters, interrupt service |
| Network | Virtual CAN socket open/bind/read/write, non-blocking I/O, poll loop, frame routing |
| State | Fault-confinement error-recovery FSM and ISO 11898-1 bus-off recovery |
| Application | CLI, live dashboard, CSV logging, DBC decoding, runtime metrics |

---

## Installation

### Prerequisites

- **GCC** or **Clang** (C99-compatible compiler)
- **GNU Make** or **CMake 3.10+**
- **Linux** (for SocketCAN support) Ã¢â‚¬â€ Windows/WSL and macOS use the mock telemetry fallback
- `libm` (math library, linked automatically on Unix)

### Build with Make

```bash
# Clone the repository
git clone <repository-url>
cd can-logger-decoder

# Build the library and executable
make

# Build and run all unit tests
make test

# Run the throughput/latency benchmark
make bench
```

### Build with CMake

```bash
cmake -S . -B build
cmake --build build

# Run tests
cd build && ctest

# Build benchmark
cmake --build build --target bench_decoder
```

### Clean Build Artifacts

```bash
make clean
```

---

## Usage

```bash
# Display DBC database summary and exit
./can_decoder -d sample.dbc --summary

# Run with mock telemetry, exit after 20 frames
./can_decoder -d sample.dbc --mock -c 20

# Run with mock telemetry at 100 Hz, log to CSV
./can_decoder -d sample.dbc --mock -r 100 -c 1000 -o can_log.csv

# Connect to a real SocketCAN interface
sudo ./can_decoder -i can0 -d sample.dbc -c 500

# Quiet mode (suppress live console dashboard)
./can_decoder -d sample.dbc --mock -q -c 100
```

### Command-Line Options

| Option | Description |
|--------|-------------|
| `-i, --interface <name>` | SocketCAN interface (default: `vcan0`) |
| `-d, --dbc <path>` | Path to Vector DBC database file (default: `sample.dbc`) |
| `-o, --output <path>` | Path to output CSV log file (default: `can_log.csv`) |
| `-m, --mock` | Force mock frame generator (simulated stream) |
| `-r, --rate <hz>` | Mock generator frequency in Hz (default: 10) |
| `-c, --count <num>` | Exit after N frames (0 = run forever, default: 0) |
| `-s, --summary` | Display DBC database summary and exit |
| `-q, --quiet` | Quiet mode (suppress live console tables) |
| `-h, --help` | Display help message |

---

## Testing

### Unit Tests

```bash
# Build and run all 6 test suites
make test

# Or run the automated test runner script
./scripts/run_unit_tests.sh
```

Test suites: `test_parser`, `test_state_machine`, `test_vcan`, `test_filters`, `test_metrics`, `test_router`.

### Memory Safety

```bash
# Run Valgrind memory leak checks
./scripts/run_valgrind.sh
```

### Benchmarking

```bash
# Build and run the throughput/latency benchmark
make bench

# Or run the benchmark directly with custom parameters
./build/bench_decoder --frames 100000 --rate 10000 --dbc sample.dbc

# Benchmark options:
#   --frames N     Number of frames to process (default: 100000)
#   --rate HZ      Mock generator frequency in Hz (default: 10000)
#   --dbc PATH     Path to Vector DBC database (optional)
#   --quiet        Print summary only
#   --help         Show help
```

The benchmark reports frames-per-second (FPS) throughput and per-frame latency statistics (min/avg/max microseconds).

### Linting & Formatting

```bash
# Verify code formatting matches .clang-format
./scripts/check_formatting.sh

# Run static analysis (cppcheck + clang-tidy if available)
./scripts/run_linter.sh
```

---

## Utility Scripts

### `scripts/export_logs.sh` Ã¢â‚¬â€ CAN Log Export

Export decoded CAN logs (CSV) into structured CSV or JSON archives for offline analysis.

```bash
# Export to CSV with filtering
./scripts/export_logs.sh can_log.csv --format csv --filter-id 0x123 --start-time "2026-01-01T00:00:00"

# Export to JSON archive with checksum
./scripts/export_logs.sh can_log.csv --format json --archive --checksum --output ./exports/feb2026
```

### `scripts/prune_buffer.sh` Ã¢â‚¬â€ Log Rotation & Buffer Cleanup

Manage log rotation and buffer cleanup on resource-constrained embedded nodes.

```bash
# Prune logs older than 7 days, keep max 10 rotated files
./scripts/prune_buffer.sh --dir ./logs --max-age-days 7 --max-count 10

# Rotate files larger than 10 MB, require 20% free disk space
./scripts/prune_buffer.sh --dir ./logs --max-size 10M --min-free-space 20

# Preview actions without executing
./scripts/prune_buffer.sh --dir ./logs --max-age-days 30 --dry-run
```

---

## Directory & File Structure

```text
can-logger-decoder/
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ .clang-format              # Strict C formatting standard (LLVM-based)
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ .clang-tidy                # Static analysis check set
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ .gitignore                 # Build artifact, log, and IDE exclusion rules
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ CMakeLists.txt             # CMake build configuration
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ Makefile                   # GNU Make build configuration
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ CHANGELOG.md               # Release changelog (Keep a Changelog format)
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ LICENSE                    # MIT license
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ README.md                  # This file
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ sample.dbc                 # Sample Vector DBC database
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ .github/
Ã¢â€â€š   Ã¢â€â€Ã¢â€â‚¬Ã¢â€â‚¬ workflows/
Ã¢â€â€š       Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ ci_build.yml       # Multi-compiler CI build matrix (GCC/Clang)
Ã¢â€â€š       Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ static_analysis.yml # cppcheck + clang-tidy on PRs
Ã¢â€â€š       Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ release_artifacts.yml # Tagged-release cross-compile & packaging
Ã¢â€â€š       Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ ci.yml             # Legacy CI build & test workflow
Ã¢â€â€š       Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ docs_check.yml     # Documentation integrity check
Ã¢â€â€š       Ã¢â€â€Ã¢â€â‚¬Ã¢â€â‚¬ test_coverage.yml  # Test coverage workflow
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ docs/
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ CI_CD_PIPELINE.md      # CI/CD pipeline architecture guide
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ ARCHITECTURE.mmd       # Mermaid system architecture diagram
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ STATE_MACHINE_DIAGRAM.mmd # Mermaid FSM state diagram
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ API_REFERENCE.md       # C library API reference
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ CONTRIBUTORS_GUIDE.md  # Contribution standards and conventions
Ã¢â€â€š   Ã¢â€â€Ã¢â€â‚¬Ã¢â€â‚¬ ... (additional technical guides)
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ include/
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ can_core.h             # Core CAN frame/message types and constants
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ can_parser.h           # CAN bus acquisition API
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ can_filters.h          # CAN acceptance filter definitions
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ dbc_decoder.h          # Vector DBC database parser/decoder API
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ error_codes.h          # Canonical error codes (can_error_t)
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ mcp2515.h              # MCP2515 SPI hardware driver API
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ test_config.h          # Test configuration
Ã¢â€â€š   Ã¢â€â€Ã¢â€â‚¬Ã¢â€â‚¬ vcan_interface.h       # Virtual CAN SocketCAN interface API
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ src/
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ can_parser.c           # CAN bus initialization and frame reading
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ dbc_decoder.c          # DBC file parsing and signal decoding
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ main.c                 # CLI entry point and acquisition loop
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ test_decoder.c         # Decoder test harness
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ core/                  # Core library sources (frames, parser, router, DLC, CRC)
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ hardware/              # MCP2515 SPI driver sources
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ network/               # SocketCAN virtual interface sources
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ state/                 # Error confinement and bus-off recovery FSM
Ã¢â€â€š   Ã¢â€â€Ã¢â€â‚¬Ã¢â€â‚¬ utils/                 # CRC, logger, metrics utilities
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ scripts/
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ benchmark_decoder.sh   # Legacy shell-based throughput benchmark
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ check_formatting.sh    # Local clang-format verification
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ clean_test_artifacts.sh # Test artifact cleanup
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ export_logs.sh         # CAN log CSV/JSON export utility
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ generate_test_report.sh # Test report generation
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ prune_buffer.sh        # Log rotation and buffer cleanup utility
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ run_linter.sh          # Local static analysis runner (cppcheck + clang-tidy)
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ run_unit_tests.sh      # Automated unit test runner
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ run_valgrind.sh        # Valgrind memory leak checker
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ smoke_test.sh          # CLI smoke test
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ validate_docs.py       # Documentation link validator
Ã¢â€â€š   Ã¢â€â€Ã¢â€â‚¬Ã¢â€â‚¬ validate_fixtures.sh   # Test fixture checksum validator
Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ tests/
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ benchmarks/
Ã¢â€â€š   Ã¢â€â€š   Ã¢â€â€Ã¢â€â‚¬Ã¢â€â‚¬ bench_decoder.c    # Throughput & latency benchmark harness
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ edge_cases/            # Edge-case test suites
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ fixtures/              # Test fixtures (raw frames, corrupted, extended, bus-off)
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ harness/               # Mock SPI and virtual socket simulators
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ integration/           # Integration tests (MCP2515 loopback, vcan bridge)
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ memory/                # Memory leak sanitizer tests
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ performance/           # Performance tests
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ stress/                # Stress tests (CAN storm)
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ linter/                # Style compliance tests
Ã¢â€â€š   Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ test_*.c               # Unit test suites (6 core suites)
Ã¢â€â€š   Ã¢â€â€Ã¢â€â‚¬Ã¢â€â‚¬ ... (additional test files)
Ã¢â€â€Ã¢â€â‚¬Ã¢â€â‚¬ examples/
    Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ sample_telemetry_log.json   # Sample decoded telemetry export
    Ã¢â€Å“Ã¢â€â‚¬Ã¢â€â‚¬ custom_fixture_generator.py # Custom test fixture generator
    Ã¢â€â€Ã¢â€â‚¬Ã¢â€â‚¬ wiring_schematic.ascii      # ASCII wiring schematic
```

---

## Contributing

1. **Fork & Branch**: Create a dedicated feature branch from `main`:
   ```bash
   git checkout -b feat/your-feature-name
   ```

2. **Local Verification**: Before committing, run:
   ```bash
   make clean && make
   make test
   ./scripts/check_formatting.sh
   ./scripts/run_linter.sh
   ./scripts/run_valgrind.sh
   make bench
   ```

3. **Commit**: Follow [Conventional Commits](https://www.conventionalcommits.org/):
   - `feat(...)`: New feature
   - `fix(...)`: Bug fix
   - `docs(...)`: Documentation
   - `test(...)`: Test additions
   - `chore(...)`: Build/CI updates

4. **Pull Request**: Push your branch and submit a PR against `main`. Ensure all GitHub Actions CI checks pass.

---

## License

MIT License. See [LICENSE](LICENSE) for details.
