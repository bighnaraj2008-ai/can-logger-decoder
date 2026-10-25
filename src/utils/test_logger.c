#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#define TEST_LOGGER_BUFFER_SIZE 4096

typedef enum {
    TEST_LOG_DEBUG = 0,
    TEST_LOG_INFO,
    TEST_LOG_WARN,
    TEST_LOG_ERROR,
    TEST_LOG_SILENT
} test_log_level_t;

typedef struct {
    test_log_level_t level;
    char buffer[TEST_LOGGER_BUFFER_SIZE];
    size_t write_pos;
    bool echo_to_stdout;
} test_logger_t;

static test_logger_t g_test_logger = {
    .level = TEST_LOG_WARN,
    .write_pos = 0,
    .echo_to_stdout = false
};

void test_logger_init(test_log_level_t level, bool echo_stdout) {
    g_test_logger.level = level;
    g_test_logger.write_pos = 0;
    g_test_logger.echo_to_stdout = echo_stdout;
    memset(g_test_logger.buffer, 0, sizeof(g_test_logger.buffer));
}

void test_logger_log(test_log_level_t level, const char *fmt, ...) {
    if (level < g_test_logger.level) return;

    va_list args;
    va_start(args, fmt);

    char line[512];
    int len = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if (len > 0) {
        if (g_test_logger.echo_to_stdout) {
            fputs(line, stdout);
        }
        if (g_test_logger.write_pos + (size_t)len < TEST_LOGGER_BUFFER_SIZE) {
            memcpy(g_test_logger.buffer + g_test_logger.write_pos, line, (size_t)len);
            g_test_logger.write_pos += (size_t)len;
        }
    }
}

const char *test_logger_get_buffer(void) {
    return g_test_logger.buffer;
}

void test_logger_clear(void) {
    g_test_logger.write_pos = 0;
    memset(g_test_logger.buffer, 0, sizeof(g_test_logger.buffer));
}
