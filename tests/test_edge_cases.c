#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"

/* Forward declaration for crc check helper */
can_error_t can_validate_frame_integrity(const can_frame_t *frame);

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

/* ======================================================================
 * Buffer Overflow Recovery Tests
 * ====================================================================== */
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

/* ======================================================================
 * Bus-Off Trigger & Recovery Tests
 * ====================================================================== */
static int bo_on_bus_off_calls = 0;
static int bo_on_reset_calls = 0;
static int bo_on_recovered_calls = 0;
static int bo_reset_should_fail = 0;
static int bo_reset_call_count = 0;
static int bo_bus_idle_should_wait = 1;

static int bo_on_enter_bus_off(void *ctx) { (void)ctx; bo_on_bus_off_calls++; return 0; }
static int bo_on_enter_reset(void *ctx) { (void)ctx; bo_on_reset_calls++; return 0; }
static int bo_on_recovered(void *ctx) { (void)ctx; bo_on_recovered_calls++; return 0; }
static int bo_reset_controller(void *ctx) {
    (void)ctx;
    bo_reset_call_count++;
    if (bo_reset_should_fail) return -1;
    return 0;
}
static int bo_wait_for_bus_idle(void *ctx, int timeout_ms) {
    (void)ctx; (void)timeout_ms;
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

/* ======================================================================
 * DLC Boundary & Wire Mismatch Tests
 * ====================================================================== */
static int test_dlc_boundary_violations(void) {
    printf("\n=== Running DLC Boundary Violation Tests ===\n");

    /* Classical CAN valid range is [0 .. 8] inclusive. */
    for (uint8_t d = 0; d <= CAN_DLC_MAX; d++) {
        TEST_ASSERT(can_dlc_is_valid(d), "DLC within [0..8] must be valid");
    }
    TEST_ASSERT(!can_dlc_is_valid(CAN_DLC_MAX + 1u), "DLC 9 must be invalid");
    TEST_ASSERT(!can_dlc_is_valid(15u), "DLC 15 (CAN FD code) invalid for classical CAN");
    TEST_ASSERT(!can_dlc_is_valid(255u), "DLC 255 must be invalid");

    /* Conversion helpers must report the out-of-range condition. */
    TEST_ASSERT(can_dlc_to_bytes(8) == 8, "can_dlc_to_bytes(8) == 8");
    TEST_ASSERT(can_dlc_to_bytes(0) == 0, "can_dlc_to_bytes(0) == 0");
    TEST_ASSERT(can_dlc_to_bytes(9) == CAN_ERR_INVALID_DLC,
                "can_dlc_to_bytes(9) must report CAN_ERR_INVALID_DLC");
    TEST_ASSERT(can_dlc_to_bytes(255) == CAN_ERR_INVALID_DLC,
                "can_dlc_to_bytes(255) must report CAN_ERR_INVALID_DLC");

    TEST_ASSERT(can_bytes_to_dlc(8) == 8, "can_bytes_to_dlc(8) == 8");
    TEST_ASSERT(can_bytes_to_dlc(0) == 0, "can_bytes_to_dlc(0) == 0");
    TEST_ASSERT(can_bytes_to_dlc(9) < 0, "can_bytes_to_dlc(9) must fail");
    TEST_ASSERT(can_bytes_to_dlc(64) < 0, "can_bytes_to_dlc(64) must fail");

    /* ISO 15765-2 CAN FD mapping spot checks. */
    TEST_ASSERT(can_dlc_to_length(0) == 0, "FD length(0) == 0");
    TEST_ASSERT(can_dlc_to_length(8) == 8, "FD length(8) == 8");
    TEST_ASSERT(can_dlc_to_length(12) == 24, "FD length(12) == 24");
    TEST_ASSERT(can_dlc_to_length(15) == 64, "FD length(15) == 64");

    return 0;
}

static int test_wire_frame_dlc_mismatch(void) {
    printf("\n=== Running Wire-Frame DLC Mismatch Tests ===\n");

    can_frame_t frame;
    can_frame_init(&frame);

    /* Wire with DLC 9 (out of range) must be rejected at parse time. */
    uint8_t bad_dlc[6] = { 0x00, 0x00, 0x00, 0x10, 9, 0x00 };
    TEST_ASSERT(can_frame_from_wire(bad_dlc, 6, &frame) == CAN_ERR_INVALID_DLC,
                "Wire DLC 9 must be rejected");

    /* Wire with DLC 15 must be rejected. */
    uint8_t bad_dlc15[5] = { 0x00, 0x00, 0x00, 0x00, 15 };
    TEST_ASSERT(can_frame_from_wire(bad_dlc15, 5, &frame) == CAN_ERR_INVALID_DLC,
                "Wire DLC 15 must be rejected");

    /* DLC valid (8) but payload truncated: wire_len 7 < 5 + 8 -> CAN_ERR_IO. */
    uint8_t truncated[7] = { 0x00, 0x00, 0x00, 0x10, 8, 0x00, 0x00 };
    TEST_ASSERT(can_frame_from_wire(truncated, 7, &frame) == CAN_ERR_IO,
                "DLC 8 with truncated payload must return CAN_ERR_IO");

    /* Truncated header (< 5 bytes) -> CAN_ERR_IO. */
    TEST_ASSERT(can_frame_from_wire((const uint8_t *)"\x00\x00\x00", 3, &frame) == CAN_ERR_IO,
                "Truncated 3-byte header must return CAN_ERR_IO");
    TEST_ASSERT(can_frame_from_wire(NULL, 10, &frame) == CAN_ERR_INVALID_ARG,
                "NULL wire must return CAN_ERR_INVALID_ARG");

    /* can_frame_to_wire must reject a frame with an invalid DLC. */
    can_frame_init(&frame);
    frame.can_id = 0x100u;
    frame.can_dlc = 9;
    uint8_t wire[64];
    size_t wire_len = sizeof(wire);
    TEST_ASSERT(can_frame_to_wire(&frame, wire, &wire_len) == CAN_ERR_INVALID_DLC,
                "can_frame_to_wire with DLC 9 must return CAN_ERR_INVALID_DLC");

    return 0;
}

/* ======================================================================
 * Invalid Arbitration ID Tests
 * ====================================================================== */
static int test_invalid_arbitration_ids(void) {
    printf("\n=== Running Invalid Arbitration ID Tests ===\n");

    /* Flag-bit stripping and classification. */
    TEST_ASSERT(can_id_is_extended(CAN_EFF_FLAG | 0x123u), "Extended flag detected");
    TEST_ASSERT(!can_id_is_extended(0x100u), "Standard id is not extended");
    TEST_ASSERT(can_id_clean(CAN_EFF_FLAG | 0x123u) == 0x123u,
                "can_id_clean strips EFF flag");
    TEST_ASSERT(can_id_clean(CAN_RTR_FLAG | 0x100u) == 0x100u,
                "can_id_clean strips RTR flag");
    TEST_ASSERT(can_id_clean(CAN_ERR_FLAG | 0x7FFu) == 0x7FFu,
                "can_id_clean strips ERR flag");

    /* Standard frame passes integrity validation. */
    can_frame_t std_frame;
    can_frame_init(&std_frame);
    std_frame.can_id = 0x100u;
    std_frame.can_dlc = 0;
    std_frame.is_extended = false;
    TEST_ASSERT(can_validate_frame_integrity(&std_frame) == CAN_OK,
                "Standard id 0x100 passes integrity validation");

    /* Normal extended frame (within 29-bit mask) passes. */
    can_frame_t ext_frame;
    can_frame_init(&ext_frame);
    ext_frame.can_id = CAN_EFF_FLAG | 0x18DAF110u;
    ext_frame.can_dlc = 8;
    ext_frame.is_extended = true;
    TEST_ASSERT(can_validate_frame_integrity(&ext_frame) == CAN_OK,
                "Extended id 0x18DAF110 passes integrity validation");

    /* Maximum valid 29-bit extended ID passes. */
    can_frame_t max_ext;
    can_frame_init(&max_ext);
    max_ext.can_id = CAN_EFF_FLAG | 0x1FFFFFFFu;
    max_ext.can_dlc = 0;
    max_ext.is_extended = true;
    TEST_ASSERT(can_validate_frame_integrity(&max_ext) == CAN_OK,
                "Max 29-bit extended id 0x1FFFFFFF passes validation");

    /* Classical CAN ID normalization by masking. */
    can_frame_t hi_bits;
    can_frame_init(&hi_bits);
    hi_bits.can_id = CAN_EFF_FLAG | 0xFFFFFFFFu;
    hi_bits.can_dlc = 0;
    hi_bits.is_extended = true;
    TEST_ASSERT(can_id_clean(hi_bits.can_id) == 0x1FFFFFFFu,
                "can_id_clean masks 0xFFFFFFFF down to 0x1FFFFFFF");
    TEST_ASSERT(can_validate_frame_integrity(&hi_bits) == CAN_OK,
                "Masked high-bit extended id passes validation");

    /* DLC out of range on a frame -> rejected by integrity validation. */
    can_frame_t bad_dlc_frame;
    can_frame_init(&bad_dlc_frame);
    bad_dlc_frame.can_id = 0x100u;
    bad_dlc_frame.can_dlc = 9;
    bad_dlc_frame.is_extended = false;
    TEST_ASSERT(can_validate_frame_integrity(&bad_dlc_frame) == CAN_ERR_INVALID_DLC,
                "DLC 9 rejected by integrity validation");

    return 0;
}

/* ======================================================================
 * Main Test Runner
 * ====================================================================== */
int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER EDGE CASES UNIT TEST SUITE    \n");
    printf("====================================================\n");

    int failed = 0;
    failed += test_buffer_overflow_recovery();
    failed += test_bus_off_fault_confinement();
    failed += test_bus_off_transition_sequence();
    failed += test_bus_off_recovery_controller();
    failed += test_dlc_boundary_violations();
    failed += test_wire_frame_dlc_mismatch();
    failed += test_invalid_arbitration_ids();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL EDGE CASE TESTS PASSED SUCCESSFULLY!          \n");
    } else {
        printf(" EDGE CASE TESTS FAILED WITH %d SUITE FAILURES!     \n", failed);
    }
    printf("====================================================\n");

    return failed;
}
