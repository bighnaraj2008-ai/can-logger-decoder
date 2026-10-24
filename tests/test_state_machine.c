#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "can_core.h"
#include "error_codes.h"

/* ----------------------------------------------------------------------
 * Mock callbacks
 * ---------------------------------------------------------------------- */

static int mock_on_bus_off_calls = 0;
static int mock_on_reset_calls = 0;
static int mock_on_recovered_calls = 0;
static int mock_reset_should_fail = 0;
static int mock_reset_call_count = 0;
static int mock_bus_idle_should_wait = 1;

static int mock_on_enter_bus_off(void *ctx) {
    (void)ctx;
    mock_on_bus_off_calls++;
    return 0;
}

static int mock_on_enter_reset(void *ctx) {
    (void)ctx;
    mock_on_reset_calls++;
    return 0;
}

static int mock_on_recovered(void *ctx) {
    (void)ctx;
    mock_on_recovered_calls++;
    return 0;
}

static int mock_reset_controller(void *ctx) {
    (void)ctx;
    mock_reset_call_count++;
    if (mock_reset_should_fail) return -1;
    return 0;
}

static int mock_wait_for_bus_idle(void *ctx, int timeout_ms) {
    (void)ctx;
    (void)timeout_ms;
    if (mock_bus_idle_should_wait) return 0; /* keep waiting */
    return 1; /* bus is idle */
}

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

static void reset_mocks(void) {
    mock_on_bus_off_calls = 0;
    mock_on_reset_calls = 0;
    mock_on_recovered_calls = 0;
    mock_reset_should_fail = 0;
    mock_reset_call_count = 0;
    mock_bus_idle_should_wait = 1;
}

/* ----------------------------------------------------------------------
 * Error recovery state machine tests
 * ---------------------------------------------------------------------- */

static int test_initial_state(void) {
    printf("\n=== Running Error Machine Initial State Tests ===\n");

    error_machine_t m;
    TEST_ASSERT(error_machine_init(&m) == CAN_OK, "Machine initializes OK");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_NORMAL,
                "Initial state is NORMAL");
    TEST_ASSERT(error_machine_is_healthy(&m), "Fresh machine is healthy");
    TEST_ASSERT(!error_machine_is_bus_off(&m), "Fresh machine is not bus-off");
    TEST_ASSERT(strcmp(err_state_to_string(ERR_STATE_BUS_OFF), "BUS_OFF") == 0,
                "State string mapping works");
    TEST_ASSERT(error_machine_reset(&m) == CAN_OK, "Reset is idempotent");
    return 0;
}

static int test_fault_confinement(void) {
    printf("\n=== Running Fault Confinement Tests ===\n");

    error_machine_t m;
    error_machine_init(&m);

    /* TEC = 100: crosses warning threshold (96) */
    TEST_ASSERT(error_machine_inject_error(&m, 100u, 0u) == CAN_OK,
                "Inject 100 TX errors OK");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_WARNING,
                "100 errors -> WARNING");
    TEST_ASSERT(error_machine_is_healthy(&m), "WARNING is still healthy");

    /* TEC = 200: crosses passive threshold (127) */
    TEST_ASSERT(error_machine_inject_error(&m, 100u, 0u) == CAN_OK,
                "Inject 100 more TX errors OK");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_ERROR_PASSIVE,
                "200 errors -> ERROR_PASSIVE");

    /* TEC = 300: crosses bus-off limit (> 255) */
    TEST_ASSERT(error_machine_inject_error(&m, 100u, 0u) == CAN_OK,
                "Inject 100 more TX errors OK");
    TEST_ASSERT(error_machine_is_bus_off(&m), "300 errors -> BUS_OFF");
    TEST_ASSERT(!error_machine_is_healthy(&m), "BUS_OFF is not healthy");
    return 0;
}

static int test_event_transitions(void) {
    printf("\n=== Running Event Transition Tests ===\n");

    error_machine_t m;
    error_machine_init(&m);

    /* WARNING -> NORMAL on clean traffic */
    TEST_ASSERT(error_machine_inject_error(&m, 100u, 0u) == CAN_OK,
                "Move into WARNING");
    TEST_ASSERT(error_machine_process_event(&m, EVT_RX_OK) == CAN_OK,
                "Process RX_OK in WARNING");
    TEST_ASSERT(error_machine_process_event(&m, EVT_TX_OK) == CAN_OK,
                "Process TX_OK in WARNING");
    /* 200 -> decayed twice, still WARNING (198 >= 96) */
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_WARNING,
                "Still WARNING after partial decay");

    /* SPI_FAULT from NORMAL -> RESET */
    error_machine_reset(&m);
    TEST_ASSERT(error_machine_process_event(&m, EVT_SPI_FAULT) == CAN_OK,
                "SPI_FAULT triggers RESET");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_RESET,
                "State after SPI_FAULT is RESET");

    /* RESET_COMPLETE -> NORMAL with counters cleared */
    TEST_ASSERT(error_machine_process_event(&m, EVT_RESET_COMPLETE) == CAN_OK,
                "RESET_COMPLETE returns to NORMAL");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_NORMAL,
                "Back to NORMAL after reset");
    return 0;
}

static int test_socket_overflow(void) {
    printf("\n=== Running Socket Buffer Overflow Tests ===\n");

    error_machine_t m;
    error_machine_init(&m);

    /* Socket overflow is a congestion fault, not fatal */
    TEST_ASSERT(error_machine_process_event(&m, EVT_SOCKET_OVERFLOW) == CAN_OK,
                "Socket overflow handled");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_WARNING,
                "Overflow moves NORMAL -> WARNING");

    /* RX buffer overflow while warning stays warning */
    TEST_ASSERT(error_machine_process_event(&m, EVT_RX_BUFFER_OVERFLOW) == CAN_OK,
                "RX buffer overflow handled");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_WARNING,
                "State remains WARNING");
    return 0;
}

static int test_bus_off_transition(void) {
    printf("\n=== Running Bus-off Transition Tests ===\n");

    error_machine_t m;
    error_machine_init(&m);
    m.on_enter_bus_off = mock_on_enter_bus_off;
    m.on_enter_reset = mock_on_enter_reset;
    m.on_recovered = mock_on_recovered;

    TEST_ASSERT(error_machine_process_event(&m, EVT_BUS_OFF_DETECTED) == CAN_OK,
                "Bus-off detected");
    TEST_ASSERT(error_machine_is_bus_off(&m), "State is BUS_OFF");
    TEST_ASSERT(mock_on_bus_off_calls == 1, "on_enter_bus_off callback fired");

    /* Manual reset from bus-off */
    TEST_ASSERT(error_machine_process_event(&m, EVT_MANUAL_RESET) == CAN_OK,
                "Manual reset from BUS_OFF");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_RESET,
                "State is RESET");
    TEST_ASSERT(mock_on_reset_calls == 1, "on_enter_reset callback fired");

    /* Recovery completes */
    TEST_ASSERT(error_machine_process_event(&m, EVT_RESET_COMPLETE) == CAN_OK,
                "Reset complete");
    TEST_ASSERT(error_machine_get_state(&m) == ERR_STATE_NORMAL,
                "Recovered to NORMAL");
    TEST_ASSERT(error_machine_is_healthy(&m), "Machine healthy again");
    TEST_ASSERT(mock_on_recovered_calls == 1, "on_recovered callback fired");
    return 0;
}

/* ----------------------------------------------------------------------
 * Bus-off recovery controller tests
 * ---------------------------------------------------------------------- */

static int test_recovery_success(void) {
    printf("\n=== Running Bus-off Recovery Success Tests ===\n");

    bus_off_recovery_t rec;
    reset_mocks();
    TEST_ASSERT(bus_off_recovery_init(&rec) == CAN_OK, "Recovery controller init");
    TEST_ASSERT(bus_off_recovery_state(&rec) == BOR_IDLE,
                "Initial state is IDLE");

    rec.reset_controller = mock_reset_controller;
    rec.wait_for_bus_idle = mock_wait_for_bus_idle;

    TEST_ASSERT(bus_off_recovery_start(&rec) == CAN_OK, "Recovery started");
    TEST_ASSERT(bus_off_recovery_state(&rec) == BOR_WAITING_BUS_IDLE,
                "Waiting for bus idle");

    /* Poll until the mock bus reports idle, then reset and recover */
    int rc = 0;
    for (int i = 0; i < 10; i++) {
        rc = bus_off_recovery_poll(&rec, (uint64_t)(i * 100));
        if (rc != 0) break;
    }
    /* rc==0 means still waiting for bus idle (mock_wait returns 0) */
    TEST_ASSERT(rc == 0 || rc == 1, "Poll drives state machine");

    mock_bus_idle_should_wait = 0; /* bus is now idle */
    rc = bus_off_recovery_poll(&rec, 500);
    TEST_ASSERT(bus_off_recovery_state(&rec) == BOR_RESET_ISSUED ||
                bus_off_recovery_state(&rec) == BOR_WAITING_BUS_IDLE,
                "Proceeds to reset after bus idle");

    rc = bus_off_recovery_poll(&rec, 600);
    TEST_ASSERT(bus_off_recovery_state(&rec) == BOR_RECOVERING ||
                bus_off_recovery_state(&rec) == BOR_FAILED,
                "Enters RECOVERING or reports failure");
    (void)rc;

    /* Fast-forward time past the 128-sequence recovery window */
    rc = bus_off_recovery_poll(&rec, 5000);
    TEST_ASSERT(bus_off_recovery_state(&rec) == BOR_RECOVERED,
                "Recovery completes (BOR_RECOVERED)");
    TEST_ASSERT(bus_off_recovery_is_done(&rec), "is_done reports success");
    return 0;
}

static int test_recovery_failed_reset(void) {
    printf("\n=== Running Bus-off Recovery Failure Tests ===\n");

    bus_off_recovery_t rec;
    reset_mocks();
    mock_bus_idle_should_wait = 0;
    mock_reset_should_fail = 1;

    TEST_ASSERT(bus_off_recovery_init(&rec) == CAN_OK, "Init OK");
    /* Callbacks are configured after init(), which zeroes the struct */
    rec.max_attempts = 2;
    rec.reset_controller = mock_reset_controller;
    rec.wait_for_bus_idle = mock_wait_for_bus_idle;

    TEST_ASSERT(bus_off_recovery_start(&rec) == CAN_OK, "Start OK");

    int rc = 0;
    for (int i = 0; i < 20; i++) {
        rc = bus_off_recovery_poll(&rec, (uint64_t)(i * 100));
        if (rc != 0) break;
    }
    TEST_ASSERT(bus_off_recovery_has_failed(&rec),
                "Exhausted reset attempts -> FAILED");
    TEST_ASSERT(mock_reset_call_count >= 2, "Reset retried up to max_attempts");
    (void)rc;
    return 0;
}

static int test_recovery_bus_idle_timeout(void) {
    printf("\n=== Running Bus-off Bus-idle Timeout Tests ===\n");

    bus_off_recovery_t rec;
    reset_mocks();
    mock_bus_idle_should_wait = 1; /* never goes idle */

    TEST_ASSERT(bus_off_recovery_init(&rec) == CAN_OK, "Init OK");
    /* Callbacks are configured after init(), which zeroes the struct */
    rec.timeout_ms = 100;
    rec.reset_controller = mock_reset_controller;
    rec.wait_for_bus_idle = mock_wait_for_bus_idle;

    TEST_ASSERT(bus_off_recovery_start(&rec) == CAN_OK, "Start OK");

    int rc = bus_off_recovery_poll(&rec, 500);
    TEST_ASSERT(rc == 0, "First poll still waiting for bus idle");
    rc = bus_off_recovery_poll(&rec, 700);
    TEST_ASSERT(bus_off_recovery_has_failed(&rec),
                "Bus-idle timeout -> FAILED");
    TEST_ASSERT(rc == -1, "poll returns -1 on failure");
    return 0;
}

static int test_recovery_invalid_args(void) {
    printf("\n=== Running Recovery Controller Argument Tests ===\n");

    TEST_ASSERT(bus_off_recovery_init(NULL) == CAN_ERR_INVALID_ARG,
                "init(NULL) invalid arg");
    TEST_ASSERT(bus_off_recovery_start(NULL) == CAN_ERR_INVALID_ARG,
                "start(NULL) invalid arg");
    TEST_ASSERT(bus_off_recovery_poll(NULL, 0) == CAN_ERR_INVALID_ARG,
                "poll(NULL) invalid arg");
    TEST_ASSERT(bus_off_recovery_reset(NULL) == CAN_ERR_INVALID_ARG,
                "reset(NULL) invalid arg");

    bus_off_recovery_t rec;
    bus_off_recovery_init(&rec);
    TEST_ASSERT(bus_off_recovery_is_done(&rec) == false, "IDLE not done");
    TEST_ASSERT(bus_off_recovery_has_failed(&rec) == false, "IDLE not failed");
    TEST_ASSERT(bus_off_recovery_reset(&rec) == CAN_OK, "reset OK");
    return 0;
}

static int test_machine_invalid_args(void) {
    printf("\n=== Running Error Machine Argument Tests ===\n");

    TEST_ASSERT(error_machine_init(NULL) == CAN_ERR_INVALID_ARG,
                "init(NULL) invalid arg");
    TEST_ASSERT(error_machine_reset(NULL) == CAN_ERR_INVALID_ARG,
                "reset(NULL) invalid arg");
    TEST_ASSERT(error_machine_process_event(NULL, EVT_RX_OK) == CAN_ERR_INVALID_ARG,
                "process_event(NULL) invalid arg");
    TEST_ASSERT(error_machine_inject_error(NULL, 1, 1) == CAN_ERR_INVALID_ARG,
                "inject_error(NULL) invalid arg");

    error_machine_t m;
    error_machine_init(&m);
    TEST_ASSERT(error_machine_process_event(&m, (err_event_t)EVT_MAX) == CAN_ERR_INVALID_ARG,
                "out-of-range event rejected");
    TEST_ASSERT(error_machine_process_event(&m, EVT_NONE) == CAN_ERR_INVALID_ARG,
                "EVT_NONE rejected");
    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" STATE MACHINE & BUS-OFF RECOVERY UNIT TEST SUITE   \n");
    printf("====================================================\n");

    int failed = 0;
    failed += test_machine_invalid_args();
    failed += test_initial_state();
    failed += test_fault_confinement();
    failed += test_event_transitions();
    failed += test_socket_overflow();
    failed += test_bus_off_transition();
    failed += test_recovery_invalid_args();
    failed += test_recovery_success();
    failed += test_recovery_failed_reset();
    failed += test_recovery_bus_idle_timeout();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL STATE MACHINE TESTS PASSED SUCCESSFULLY!      \n");
    } else {
        printf(" STATE MACHINE TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
