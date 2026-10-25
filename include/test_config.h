#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* General test harness timeout limits (milliseconds) */
#define TEST_DEFAULT_TIMEOUT_MS     1000
#define TEST_SHORT_TIMEOUT_MS        100
#define TEST_LONG_TIMEOUT_MS        5000

/* Stress and performance test parameters */
#define TEST_STRESS_FRAME_COUNT    50000
#define TEST_PERF_ITERATION_COUNT 100000
#define TEST_PERF_MIN_FPS         100000.0

/* Fixture limits and buffer sizes */
#define TEST_MAX_FIXTURE_PATH_LEN    256
#define TEST_WIRE_BUFFER_MAX_LEN      64
#define TEST_LOG_RING_BUFFER_CAP    1024

/* Feature flags for test environment */
#define TEST_ENABLE_VERBOSE_LOGS       0
#define TEST_ENABLE_COLOR_OUTPUT       1

#endif /* TEST_CONFIG_H */
