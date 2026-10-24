#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "mcp2515.h"
#include "can_filters.h"
#include "error_codes.h"

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

/* Declare filter register helper prototypes */
int mcp2515_set_filter_registers(mcp2515_t *dev, uint8_t filter_idx, uint32_t id, bool is_extended);
int mcp2515_set_mask_registers(mcp2515_t *dev, uint8_t mask_idx, uint32_t mask, bool is_extended);

static int test_standard_filter_setup(void) {
    printf("\n=== Running Standard ID Filter Register Tests ===\n");

    mcp2515_t *dev = NULL;
    int rc = mcp2515_create(&dev, NULL, NULL);
    TEST_ASSERT(rc == CAN_OK && dev != NULL, "MCP2515 device created");

    /* Program filter 0 with standard CAN ID 0x7DF */
    rc = mcp2515_set_filter_registers(dev, 0, 0x7DFu, false);
    TEST_ASSERT(rc == CAN_OK, "Filter registers programmed successfully");

    uint8_t sidh = 0, sidl = 0;
    mcp2515_read_reg(dev, MCP2515_RXF0SIDH, &sidh);
    mcp2515_read_reg(dev, MCP2515_RXF0SIDL, &sidl);

    /* 0x7DF: high 8 bits (0x7DF >> 3 = 0xFB), low 3 bits ((0x7DF << 5) & 0xE0 = 0xE0) */
    TEST_ASSERT(sidh == 0xFBu, "SIDH matches 0xFB for standard ID 0x7DF");
    TEST_ASSERT((sidl & 0xE0u) == 0xE0u, "SIDL high bits match 0xE0");
    TEST_ASSERT((sidl & 0x08u) == 0x00u, "EXIDE bit is 0 for standard filter");

    mcp2515_destroy(dev);
    return 0;
}

static int test_extended_filter_setup(void) {
    printf("\n=== Running Extended ID Filter Register Tests ===\n");

    mcp2515_t *dev = NULL;
    mcp2515_create(&dev, NULL, NULL);

    /* Program filter 1 with extended CAN ID 0x18DAF110 */
    uint32_t ext_id = 0x18DAF110u;
    int rc = mcp2515_set_filter_registers(dev, 1, ext_id, true);
    TEST_ASSERT(rc == CAN_OK, "Extended filter programmed");

    uint8_t sidl = 0;
    mcp2515_read_reg(dev, MCP2515_RXF1SIDL, &sidl);
    TEST_ASSERT((sidl & 0x08u) != 0u, "EXIDE bit is set for extended filter");

    mcp2515_destroy(dev);
    return 0;
}

static int test_filter_set_configuration(void) {
    printf("\n=== Running Acceptance Filter Set Configuration Tests ===\n");

    mcp2515_t *dev = NULL;
    mcp2515_create(&dev, NULL, NULL);

    can_filter_set_t filter_set;
    can_filter_set_init(&filter_set);
    can_filter_set_add(&filter_set, 0x100u, 0x7F0u, CAN_FILTER_MODE_STANDARD);

    int rc = mcp2515_configure_acceptance_filters(dev, &filter_set);
    TEST_ASSERT(rc == CAN_OK, "Acceptance filter configured");

    /* Clear via promiscuous mode */
    filter_set.promiscuous = true;
    rc = mcp2515_configure_acceptance_filters(dev, &filter_set);
    TEST_ASSERT(rc == CAN_OK, "Promiscuous configuration cleared masks");

    mcp2515_destroy(dev);
    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" MCP2515 HARDWARE FILTER UNIT TEST SUITE            \n");
    printf("====================================================\n");

    int failed = 0;
    failed += test_standard_filter_setup();
    failed += test_extended_filter_setup();
    failed += test_filter_set_configuration();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL HARDWARE FILTER TESTS PASSED!                 \n");
    } else {
        printf(" HARDWARE FILTER FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
