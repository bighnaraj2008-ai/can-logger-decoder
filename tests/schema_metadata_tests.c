#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
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

static int test_can_core_constants_and_types(void) {
    printf("\n=== Running CAN Core Schema & Metadata Tests ===\n");

    /* ISO 11898-1 Identifier Masks */
    TEST_ASSERT(CAN_SFF_MASK == 0x000007FFu, "Standard frame mask is 11 bits (0x7FF)");
    TEST_ASSERT(CAN_EFF_MASK == 0x1FFFFFFFu, "Extended frame mask is 29 bits (0x1FFFFFFF)");
    TEST_ASSERT(CAN_ERR_MASK == 0x1FFFFFFFu, "Error frame mask is 29 bits");

    /* Flag bit positions */
    TEST_ASSERT(CAN_EFF_FLAG == 0x80000000u, "EFF flag is bit 31");
    TEST_ASSERT(CAN_RTR_FLAG == 0x40000000u, "RTR flag is bit 30");
    TEST_ASSERT(CAN_ERR_FLAG == 0x20000000u, "ERR flag is bit 29");

    /* Data length limits */
    TEST_ASSERT(CAN_DLC_MAX == 8, "Classical CAN maximum DLC is 8");
    TEST_ASSERT(CAN_FRAME_MAX_DATA_LEN == 8, "Maximum payload buffer is 8 bytes");

    /* Error codes */
    TEST_ASSERT(CAN_OK == 0, "CAN_OK is 0");
    TEST_ASSERT(can_error_str(CAN_OK) != NULL, "CAN_OK description string is defined");
    TEST_ASSERT(can_error_str(CAN_ERR_INVALID_DLC) != NULL, "CAN_ERR_INVALID_DLC description defined");

    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER SCHEMA & METADATA TEST SUITE  \n");
    printf("====================================================\n");

    int failed = test_can_core_constants_and_types();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL SCHEMA & METADATA TESTS PASSED SUCCESSFULLY!   \n");
    } else {
        printf(" SCHEMA & METADATA TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
