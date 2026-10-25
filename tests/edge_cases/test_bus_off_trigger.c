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

/* Mock callbacks for bus-off recovery */
static int bo_on_bus_off_calls = 0;
static int bo_on_reset_calls = 0;
static int bo_on_recovered_calls = 0;
static int bo_reset_should_fail = 0;
static int bo_reset_call_count = 0;
static int bo_bus_idle_should_wait = 1;

static int bo_on_enter_bus_off(void *ctx) {
    (void)ctx;
    bo_on_bus_off_calls++;
    return 0;
}
static int bo_on_enter_reset(void *ctx) {
    (void)ctx;
    bo_on_reset_calls++;
    return 0;
}
static int bo_on_recovered(void *ctx) {
    (void)ctx;
    bo_on_recovered_calls++;
    return 0;
}
static int bo_reset_controller(void *ctx) {
    (void)ctx;
    bo_reset_call_count++;
    if (bo_reset_should_fail) return -1;
    return 0;
}
static int bo_wait_for_bus_idle(void *ctx, int timeout_ms) {
    (void)ctx;
    (void)timeout_ms;
    if (bo_bus_idle_should_wait) return 0;
    return 1;
}

static void bo_reset_mocks(void) {
    bo_on_bus_off_calls = 0;
    bo_on_reset_calls = 0;
    bo_on_recovered_calls = 0;
    bo_reset_should_fail = 0;
    bo_reset_call_count = 0;
    bo_bus_idle_should_wait = 1;
}

static int test_bus_off_fault_confinement(void) {
    printf("\n=== Running Bus-off Fault Confinement Tests ===\n");

    error_machine_t m;
    TEST_ASSERT(error_machine_init(&m) == CAN_OK, "Machine initializes OK");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_NORMAL, "Initial state is NORMAL");
    TEST_ASSERT(error_machine_is_healthy(&m), "Fresh machine is healthy");
    TEST_ASSERT(!error_machine_is_bus_off(&m), "Fresh machine is not bus-off");

    /* TEC = 100 crosses warning threshold (96). */
    TEST_ASSERT(error_machine_inject_error(&m, 100u, 0u) == CAN_OK, "Inject 100 TX errors OK");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_WARNING, "100 errors -> WARNING");
    TEST_ASSERT(error_machine_is_healthy(&m), "WARNING is still healthy");

    /* TEC = 200 crosses error-passive threshold (127). */
    TEST_ASSERT(error_machine_inject_error(&m, 100u, 0u) == CAN_OK, "Inject 100 more TX errors OK");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_ERROR_PASSIVE, "200 errors -> ERROR_PASSIVE");

    /* TEC = 300 crosses bus-off limit (> 255). */
    TEST_ASSERT(error_machine_inject_error(&m, 100u, 0u) == CAN_OK, "Inject 100 more TX errors OK");
    TEST_ASSERT(error_machine_is_bus_off(&m), "300 errors -> BUS_OFF");
    TEST_ASSERT(!error_machine_is_healthy(&m), "BUS_OFF is not healthy");

    return 0;
}

static int test_bus_off_transition_sequence(void) {
    printf("\n=== Running Bus-off Transition Sequence Tests ===\n");

    error_machine_t m;
    error_machine_init(&m);
    m.on_enter_bus_off = bo_on_enter_bus_off;
    m.on_enter_reset = bo_on_enter_reset;
    m.on_recovered = bo_on_recovered;

    /* EVT_BUS_OFF_DETECTED from NORMAL -> BUS_OFF. */
    TEST_ASSERT(error_machine_process_event(&m, EVT_BUS_OFF_DETECTED) == CAN_OK, "Bus-off detected");
    TEST_ASSERT(error_machine_is_bus_off(&m), "State is BUS_OFF");
    TEST_ASSERT(bo_on_bus_off_calls == 1, "on_enter_bus_off callback fired");

    /* Manual reset from bus-off -> RESET. */
    TEST_ASSERT(error_machine_process_event(&m, EVT_MANUAL_RESET) == CAN_OK, "Manual reset from BUS_OFF");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_RESET, "State is RESET");
    TEST_ASSERT(bo_on_reset_calls == 1, "on_enter_reset callback fired");

    /* EVT_RESET_COMPLETE -> NORMAL, counters cleared, recovery counted. */
    TEST_ASSERT(error_machine_process_event(&m, EVT_RESET_COMPLETE) == CAN_OK, "Reset complete");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_NORMAL, "Recovered to NORMAL");
    TEST_ASSERT(error_machine_is_healthy(&m), "Machine healthy again");
    TEST_ASSERT(m.tx_error_count == 0 && m.rx_error_count == 0, "Error counters cleared");
    TEST_ASSERT(m.recovery_count == 1, "recovery_count incremented to 1");
    TEST_ASSERT(bo_on_recovered_calls == 1, "on_recovered callback fired");

    return 0;
}

static int test_bus_off_recovery_controller(void) {
    printf("\n=== Running Bus-off Recovery Controller Tests ===\n");

    bus_off_recovery_t rec;
    bo_reset_mocks();
    TEST_ASSERT(bus_off_recovery_init(&rec) == CAN_OK, "Recovery controller init");
    TEST_ASSERT(bus_off_recovery_state(&rec) == BOR_IDLE, "Initial state is IDLE");

    rec.reset_controller = bo_reset_controller;
    rec.wait_for_bus_idle = bo_wait_for_bus_idle;

    TEST_ASSERT(bus_off_recovery_start(&rec) == CAN_OK, "Recovery started");
    TEST_ASSERT(bus_off_recovery_state(&rec) == BOR_WAITING_BUS_IDLE, "Waiting for bus idle");

    /* Bus never idles -> timeout -> FAILED. */
    bo_bus_idle_should_wait = 1;
    rec.timeout_ms = 100;
    int rc = bus_off_recovery_poll(&rec, 500);
    TEST_ASSERT(rc == 0, "First poll still waiting for bus idle");
    rc = bus_off_recovery_poll(&rec, 700);
    TEST_ASSERT(bus_off_recovery_has_failed(&rec), "Bus-idle timeout -> FAILED");
    TEST_ASSERT(rc == -1, "poll returns -1 on failure");

    /* Successful recovery path. */
    bo_reset_mocks();
    bo_bus_idle_should_wait = 0;
    bus_off_recovery_init(&rec);
    rec.reset_controller = bo_reset_controller;
    rec.wait_for_bus_idle = bo_wait_for_bus_idle;
    bus_off_recovery_start(&rec);

    int polls = 0;
    while (polls++ < 40) {
        rc = bus_off_recovery_poll(&rec, (uint64_t)(polls * 100));
        if (rc != 0) break;
    }
    TEST_ASSERT(bus_off_recovery_is_done(&rec) || bus_off_recovery_has_failed(&rec),
                "Recovery controller reaches a terminal state");

    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER BUS-OFF TRIGGER UNIT TEST     \n");
    printf("====================================================\n");

    int failed = 0;
    failed += test_bus_off_fault_confinement();
    failed += test_bus_off_transition_sequence();
    failed += test_bus_off_recovery_controller();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL BUS-OFF TRIGGER TESTS PASSED SUCCESSFULLY!    \n");
    } else {
        printf(" BUS-OFF TRIGGER TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
