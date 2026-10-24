cmake_minimum_required(VERSION 3.10)
project(can-logger-decoder C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

include_directories(include)

# Core library sources
set(CORE_SOURCES
    src/core/can_parser.c
    src/core/can_frame.c
    src/core/dlc_validator.c
    src/core/can_router.c
    src/hardware/mcp2515_spi.c
    src/hardware/mcp2515_init.c
    src/hardware/mcp2515_interrupt.c
    src/hardware/mcp2515_filters.c
    src/network/vcan_socket.c
    src/network/vcan_listener.c
    src/state/error_machine.c
    src/state/bus_off_recovery.c
    src/utils/logger.c
    src/utils/crc_check.c
    src/utils/metrics.c
    src/dbc_decoder.c
)

add_library(can_core_lib STATIC ${CORE_SOURCES})
if(UNIX)
    target_link_libraries(can_core_lib m)
endif()

# Main executable
add_executable(can_decoder src/main.c)
target_link_libraries(can_decoder can_core_lib)

# Testing configuration
enable_testing()

set(TEST_SUITES
    test_parser
    test_state_machine
    test_vcan
    test_filters
    test_metrics
    test_router
    test_decoder
)

foreach(TEST_NAME ${TEST_SUITES})
    add_executable(${TEST_NAME} tests/${TEST_NAME}.c)
    target_link_libraries(${TEST_NAME} can_core_lib)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
