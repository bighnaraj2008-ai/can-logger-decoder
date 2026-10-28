/* Refined harness assertion formatting with explicit status tracking */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t total_assertions;
    uint32_t passed_assertions;
    uint32_t failed_assertions;
} harness_stats_t;

static harness_stats_t g_harness_stats = {0, 0, 0};

void harness_reset_stats(void) {
    g_harness_stats.total_assertions = 0;
    g_harness_stats.passed_assertions = 0;
    g_harness_stats.failed_assertions = 0;
}

int harness_assert(bool condition, const char *msg, const char *file, int line) {
    g_harness_stats.total_assertions++;
    if (condition) {
        g_harness_stats.passed_assertions++;
        printf("  [PASS] %s\n", msg);
        return 0;
    } else {
        g_harness_stats.failed_assertions++;
        printf("[FAIL] %s:%d: %s\n", file, line, msg);
        return 1;
    }
}

int harness_assert_equal_int(int actual, int expected, const char *msg, const char *file, int line) {
    g_harness_stats.total_assertions++;
    if (actual == expected) {
        g_harness_stats.passed_assertions++;
        printf("  [PASS] %s (got %d)\n", msg, actual);
        return 0;
    } else {
        g_harness_stats.failed_assertions++;
        printf("[FAIL] %s:%d: %s (expected %d, got %d)\n", file, line, msg, expected, actual);
        return 1;
    }
}

int harness_assert_mem_equal(const void *actual, const void *expected, size_t len,
                             const char *msg, const char *file, int line) {
    g_harness_stats.total_assertions++;
    if (memcmp(actual, expected, len) == 0) {
        g_harness_stats.passed_assertions++;
        printf("  [PASS] %s\n", msg);
        return 0;
    } else {
        g_harness_stats.failed_assertions++;
        printf("[FAIL] %s:%d: %s (memory mismatch over %zu bytes)\n", file, line, msg, len);
        return 1;
    }
}

uint32_t harness_get_total_assertions(void) {
    return g_harness_stats.total_assertions;
}

uint32_t harness_get_passed_assertions(void) {
    return g_harness_stats.passed_assertions;
}

uint32_t harness_get_failed_assertions(void) {
    return g_harness_stats.failed_assertions;
}

void harness_print_summary(const char *suite_name) {
    printf("\n====================================================\n");
    printf(" SUITE SUMMARY: %s\n", suite_name ? suite_name : "Test Suite");
    printf(" Total Assertions:  %u\n", g_harness_stats.total_assertions);
    printf(" Passed Assertions: %u\n", g_harness_stats.passed_assertions);
    printf(" Failed Assertions: %u\n", g_harness_stats.failed_assertions);
    printf(" Pass Rate:         %.1f%%\n",
           g_harness_stats.total_assertions > 0
               ? (100.0 * g_harness_stats.passed_assertions / g_harness_stats.total_assertions)
               : 100.0);
    printf("====================================================\n");
}
