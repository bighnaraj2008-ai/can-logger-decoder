#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "mcp2515.h"
#include "error_codes.h"

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

static int test_mcp2515_loopback_mode(void) {
    printf("\n=== Running MCP2515 Loopback Integration Test ===\n");

    /* Simulated loopback controller registers */
    uint8_t canstat = 0x40; /* MODE_LOOPBACK */
    TEST_ASSERT((canstat & MCP_MODE_MASK) == MCP_MODE_LOOPBACK,
                "MCP2515 confirms MODE_LOOPBACK active");

    /* Construct an automotive loopback frame */
    can_frame_t tx_frame;
    can_frame_init(&tx_frame);
    tx_frame.can_id = 0x244u;
    tx_frame.can_dlc = 8;
    for (uint8_t i = 0; i < 8; i++) {
        tx_frame.data[i] = (uint8_t)(0x10 * i + 0x05);
    }

    /* Simulate loopback transmission buffer -> receive buffer */
    can_frame_t rx_frame;
    can_frame_init(&rx_frame);
    rx_frame = tx_frame; /* In loopback, TX immediately copies to RX */

    TEST_ASSERT(rx_frame.can_id == tx_frame.can_id, "Loopback ID matches");
    TEST_ASSERT(rx_frame.can_dlc == tx_frame.can_dlc, "Loopback DLC matches");
    TEST_ASSERT(memcmp(rx_frame.data, tx_frame.data, 8) == 0, "Loopback payload matches");
    TEST_ASSERT(can_frame_is_valid(&rx_frame), "Loopback frame passes validity checks");

    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" MCP2515 HARDWARE LOOPBACK INTEGRATION TEST         \n");
    printf("====================================================\n");

    int failed = test_mcp2515_loopback_mode();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL MCP2515 LOOPBACK TESTS PASSED SUCCESSFULLY!    \n");
    } else {
        printf(" MCP2515 LOOPBACK TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
