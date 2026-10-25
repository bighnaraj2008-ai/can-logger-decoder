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
                "can_frame_to_wire must reject invalid DLC 9");

    /* Valid wire round-trip works cleanly. */
    can_frame_init(&frame);
    frame.can_id = 0x100u;
    frame.can_dlc = 4;
    frame.data[0] = 0xDE; frame.data[1] = 0xAD;
    frame.data[2] = 0xBE; frame.data[3] = 0xEF;
    wire_len = sizeof(wire);
    TEST_ASSERT(can_frame_to_wire(&frame, wire, &wire_len) == CAN_OK,
                "can_frame_to_wire succeeds for valid DLC 4");
    TEST_ASSERT(wire_len == 9u, "Wire length = 4 id + 1 dlc + 4 data");

    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER DLC MISMATCH UNIT TEST        \n");
    printf("====================================================\n");

    int failed = 0;
    failed += test_dlc_boundary_violations();
    failed += test_wire_frame_dlc_mismatch();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL DLC MISMATCH TESTS PASSED SUCCESSFULLY!       \n");
    } else {
        printf(" DLC MISMATCH TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
