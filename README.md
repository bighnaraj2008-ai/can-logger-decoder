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

- **`docs/ARCHITECTURE.mmd`** â€” System component architecture diagram
- **`docs/STATE_MACHINE_DIAGRAM.mmd`** â€” Fault confinement FSM state diagram

### Architecture Overview

```
                        +---------------------------+
                        |     Application Layer     |
                        |  CLI â€¢ Dashboard â€¢ CSV    |
                        |  DBC Decoder â€¢ Runtime    |
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
- **Linux** (for SocketCAN support) â€” Windows/WSL and macOS use the mock telemetry fallback
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

### `scripts/export_logs.sh` â€” CAN Log Export

Export decoded CAN logs (CSV) into structured CSV or JSON archives for offline analysis.

```bash
# Export to CSV with filtering
./scripts/export_logs.sh can_log.csv --format csv --filter-id 0x123 --start-time "2026-01-01T00:00:00"

# Export to JSON archive with checksum
./scripts/export_logs.sh can_log.csv --format json --archive --checksum --output ./exports/feb2026
```

### `scripts/prune_buffer.sh` â€” Log Rotation & Buffer Cleanup

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
â”œâ”€â”€ .clang-format              # Strict C formatting standard (LLVM-based)
â”œâ”€â”€ .clang-tidy                # Static analysis check set
â”œâ”€â”€ .gitignore                 # Build artifact, log, and IDE exclusion rules
â”œâ”€â”€ CMakeLists.txt             # CMake build configuration
â”œâ”€â”€ Makefile                   # GNU Make build configuration
â”œâ”€â”€ CHANGELOG.md               # Release changelog (Keep a Changelog format)
â”œâ”€â”€ LICENSE                    # MIT license
â”œâ”€â”€ README.md                  # This file
â”œâ”€â”€ sample.dbc                 # Sample Vector DBC database
â”œâ”€â”€ .github/
â”‚   â””â”€â”€ workflows/
â”‚       â”œâ”€â”€ ci_build.yml       # Multi-compiler CI build matrix (GCC/Clang)
â”‚       â”œâ”€â”€ static_analysis.yml # cppcheck + clang-tidy on PRs
â”‚       â”œâ”€â”€ release_artifacts.yml # Tagged-release cross-compile & packaging
â”‚       â”œâ”€â”€ ci.yml             # Legacy CI build & test workflow
â”‚       â”œâ”€â”€ docs_check.yml     # Documentation integrity check
â”‚       â””â”€â”€ test_coverage.yml  # Test coverage workflow
â”œâ”€â”€ docs/
â”‚   â”œâ”€â”€ CI_CD_PIPELINE.md      # CI/CD pipeline architecture guide
â”‚   â”œâ”€â”€ ARCHITECTURE.mmd       # Mermaid system architecture diagram
â”‚   â”œâ”€â”€ STATE_MACHINE_DIAGRAM.mmd # Mermaid FSM state diagram
â”‚   â”œâ”€â”€ API_REFERENCE.md       # C library API reference
â”‚   â”œâ”€â”€ CONTRIBUTORS_GUIDE.md  # Contribution standards and conventions
â”‚   â””â”€â”€ ... (additional technical guides)
â”œâ”€â”€ include/
â”‚   â”œâ”€â”€ can_core.h             # Core CAN frame/message types and constants
â”‚   â”œâ”€â”€ can_parser.h           # CAN bus acquisition API
â”‚   â”œâ”€â”€ can_filters.h          # CAN acceptance filter definitions
â”‚   â”œâ”€â”€ dbc_decoder.h          # Vector DBC database parser/decoder API
â”‚   â”œâ”€â”€ error_codes.h          # Canonical error codes (can_error_t)
â”‚   â”œâ”€â”€ mcp2515.h              # MCP2515 SPI hardware driver API
â”‚   â”œâ”€â”€ test_config.h          # Test configuration
â”‚   â””â”€â”€ vcan_interface.h       # Virtual CAN SocketCAN interface API
â”œâ”€â”€ src/
â”‚   â”œâ”€â”€ can_parser.c           # CAN bus initialization and frame reading
â”‚   â”œâ”€â”€ dbc_decoder.c          # DBC file parsing and signal decoding
â”‚   â”œâ”€â”€ main.c                 # CLI entry point and acquisition loop
â”‚   â”œâ”€â”€ test_decoder.c         # Decoder test harness
â”‚   â”œâ”€â”€ core/                  # Core library sources (frames, parser, router, DLC, CRC)
â”‚   â”œâ”€â”€ hardware/              # MCP2515 SPI driver sources
â”‚   â”œâ”€â”€ network/               # SocketCAN virtual interface sources
â”‚   â”œâ”€â”€ state/                 # Error confinement and bus-off recovery FSM
â”‚   â””â”€â”€ utils/                 # CRC, logger, metrics utilities
â”œâ”€â”€ scripts/
â”‚   â”œâ”€â”€ benchmark_decoder.sh   # Legacy shell-based throughput benchmark
â”‚   â”œâ”€â”€ check_formatting.sh    # Local clang-format verification
â”‚   â”œâ”€â”€ clean_test_artifacts.sh # Test artifact cleanup
â”‚   â”œâ”€â”€ export_logs.sh         # CAN log CSV/JSON export utility
â”‚   â”œâ”€â”€ generate_test_report.sh # Test report generation
â”‚   â”œâ”€â”€ prune_buffer.sh        # Log rotation and buffer cleanup utility
â”‚   â”œâ”€â”€ run_linter.sh          # Local static analysis runner (cppcheck + clang-tidy)
â”‚   â”œâ”€â”€ run_unit_tests.sh      # Automated unit test runner
â”‚   â”œâ”€â”€ run_valgrind.sh        # Valgrind memory leak checker
â”‚   â”œâ”€â”€ smoke_test.sh          # CLI smoke test
â”‚   â”œâ”€â”€ validate_docs.py       # Documentation link validator
â”‚   â””â”€â”€ validate_fixtures.sh   # Test fixture checksum validator
â”œâ”€â”€ tests/
â”‚   â”œâ”€â”€ benchmarks/
â”‚   â”‚   â””â”€â”€ bench_decoder.c    # Throughput & latency benchmark harness
â”‚   â”œâ”€â”€ edge_cases/            # Edge-case test suites
â”‚   â”œâ”€â”€ fixtures/              # Test fixtures (raw frames, corrupted, extended, bus-off)
â”‚   â”œâ”€â”€ harness/               # Mock SPI and virtual socket simulators
â”‚   â”œâ”€â”€ integration/           # Integration tests (MCP2515 loopback, vcan bridge)
â”‚   â”œâ”€â”€ memory/                # Memory leak sanitizer tests
â”‚   â”œâ”€â”€ performance/           # Performance tests
â”‚   â”œâ”€â”€ stress/                # Stress tests (CAN storm)
â”‚   â”œâ”€â”€ linter/                # Style compliance tests
â”‚   â”œâ”€â”€ test_*.c               # Unit test suites (6 core suites)
â”‚   â””â”€â”€ ... (additional test files)
â””â”€â”€ examples/
    â”œâ”€â”€ sample_telemetry_log.json   # Sample decoded telemetry export
    â”œâ”€â”€ custom_fixture_generator.py # Custom test fixture generator
    â””â”€â”€ wiring_schematic.ascii      # ASCII wiring schematic
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
