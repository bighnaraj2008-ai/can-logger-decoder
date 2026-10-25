#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"

/* can_validate_frame_integrity is implemented in crc_check.c */
can_error_t can_validate_frame_integrity(const can_frame_t *frame);

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

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

int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER ARBITRATION ID UNIT TEST      \n");
    printf("====================================================\n");

    int failed = test_invalid_arbitration_ids();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL ARBITRATION ID TESTS PASSED SUCCESSFULLY!     \n");
    } else {
        printf(" ARBITRATION ID TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
