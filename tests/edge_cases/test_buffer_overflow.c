#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

static int test_buffer_overflow_recovery(void) {
    printf("\n=== Running Buffer Overflow Recovery Tests ===\n");

    error_machine_t m;
    TEST_ASSERT(error_machine_init(&m) == CAN_OK, "Error machine initialized");

    /* Socket overflow is a congestion fault: NORMAL -> WARNING, non-fatal. */
    TEST_ASSERT(error_machine_process_event(&m, EVT_SOCKET_OVERFLOW) == CAN_OK,
                "Socket overflow handled successfully");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_WARNING,
                "Overflow moves NORMAL -> WARNING");
    TEST_ASSERT(m.fault_count == 1, "fault_count incremented on overflow");

    /* RX buffer overflow while in WARNING stays WARNING. */
    TEST_ASSERT(error_machine_process_event(&m, EVT_RX_BUFFER_OVERFLOW) == CAN_OK,
                "RX buffer overflow handled successfully");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_WARNING,
                "State remains WARNING after RX buffer overflow");

    /* Clean traffic decays state back to NORMAL (recovery). */
    TEST_ASSERT(error_machine_process_event(&m, EVT_RX_OK) == CAN_OK,
                "Clean RX_OK after overflow");
    TEST_ASSERT(error_machine_process_event(&m, EVT_TX_OK) == CAN_OK,
                "Clean TX_OK after overflow");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_NORMAL,
                "State back to NORMAL after clean traffic");
    TEST_ASSERT(error_machine_is_healthy(&m), "Machine healthy after overflow recovery");

    /* Overflow from bus-off is logged but does not crash the machine. */
    error_machine_inject_error(&m, 300u, 0u); /* force BUS_OFF */
    TEST_ASSERT(error_machine_is_bus_off(&m), "Forced into BUS_OFF");
    TEST_ASSERT(error_machine_process_event(&m, EVT_SOCKET_OVERFLOW) == CAN_OK,
                "Overflow during BUS_OFF tolerated cleanly");
    TEST_ASSERT(error_machine_is_bus_off(&m), "Still BUS_OFF after overflow");

    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER BUFFER OVERFLOW RECOVERY TEST \n");
    printf("====================================================\n");

    int failed = test_buffer_overflow_recovery();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL BUFFER OVERFLOW TESTS PASSED SUCCESSFULLY!    \n");
    } else {
        printf(" BUFFER OVERFLOW TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
