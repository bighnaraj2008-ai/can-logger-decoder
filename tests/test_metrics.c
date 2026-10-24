#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"

/* Metrics structure and prototypes */
typedef struct {
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t error_frames;
    uint64_t dropped_frames;
    uint64_t bus_off_events;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    double   start_time_sec;
    double   last_update_sec;
} can_metrics_t;

void can_metrics_init(can_metrics_t *m);
void can_metrics_reset(can_metrics_t *m);
void can_metrics_record_rx(can_metrics_t *m, const can_frame_t *frame);
void can_metrics_record_tx(can_metrics_t *m, const can_frame_t *frame);
void can_metrics_record_drop(can_metrics_t *m);
void can_metrics_record_bus_off(can_metrics_t *m);
double can_metrics_get_rx_fps(const can_metrics_t *m);
double can_metrics_get_rx_kbps(const can_metrics_t *m);
void can_metrics_summary(const can_metrics_t *m, char *buf, size_t len);

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

static int test_metrics_counters(void) {
    printf("\n=== Running Telemetry Metrics Counter Tests ===\n");

    can_metrics_t m;
    can_metrics_init(&m);

    TEST_ASSERT(m.rx_frames == 0, "Initial RX frames is 0");
    TEST_ASSERT(m.tx_frames == 0, "Initial TX frames is 0");
    TEST_ASSERT(m.dropped_frames == 0, "Initial dropped frames is 0");

    can_frame_t f;
    can_frame_init(&f);
    f.can_id = 0x123;
    f.can_dlc = 8;
    f.is_error = false;

    can_metrics_record_rx(&m, &f);
    TEST_ASSERT(m.rx_frames == 1, "RX frames incremented to 1");
    TEST_ASSERT(m.rx_bytes == 8, "RX bytes recorded 8");

    can_metrics_record_drop(&m);
    can_metrics_record_drop(&m);
    TEST_ASSERT(m.dropped_frames == 2, "Dropped frames incremented to 2");

    f.is_error = true;
    can_metrics_record_rx(&m, &f);
    TEST_ASSERT(m.error_frames == 1, "Error frames incremented to 1");

    can_metrics_record_bus_off(&m);
    TEST_ASSERT(m.bus_off_events == 1, "Bus-off events recorded 1");

    char summary[128];
    can_metrics_summary(&m, summary, sizeof(summary));
    TEST_ASSERT(strlen(summary) > 0, "Metrics summary formatted");

    can_metrics_reset(&m);
    TEST_ASSERT(m.rx_frames == 0, "Reset clears RX frames");
    TEST_ASSERT(m.dropped_frames == 0, "Reset clears dropped frames");

    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" TELEMETRY METRICS UNIT TEST SUITE                  \n");
    printf("====================================================\n");

    int failed = 0;
    failed += test_metrics_counters();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL METRICS TESTS PASSED SUCCESSFULLY!             \n");
    } else {
        printf(" METRICS TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
