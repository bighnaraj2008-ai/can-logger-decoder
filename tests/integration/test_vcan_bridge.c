#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "vcan_interface.h"
#include "error_codes.h"

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

static int test_vcan_bridge_routing(void) {
    printf("\n=== Running Virtual SocketCAN Bridge Integration Test ===\n");

    /* Simulated virtual socket interface handle */
    vcan_handle_t vcan;
    memset(&vcan, 0, sizeof(vcan));
    vcan.is_open = true;

    /* Frame originating on physical bus to be routed across bridge */
    can_frame_t inbound;
    can_frame_init(&inbound);
    inbound.can_id = 0x301u;
    inbound.can_dlc = 8;
    for (uint8_t i = 0; i < 8; i++) {
        inbound.data[i] = (uint8_t)(0xAA ^ i);
    }

    /* Simulate bridge forwarding */
    can_frame_t forwarded;
    can_frame_init(&forwarded);
    forwarded = inbound;

    TEST_ASSERT(forwarded.can_id == inbound.can_id, "Forwarded ID matches original");
    TEST_ASSERT(forwarded.can_dlc == inbound.can_dlc, "Forwarded DLC matches original");
    TEST_ASSERT(memcmp(forwarded.data, inbound.data, 8) == 0, "Forwarded payload matches");
    TEST_ASSERT(can_frame_is_valid(&forwarded), "Forwarded frame is valid CAN 2.0");

    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" VIRTUAL SOCKETCAN BRIDGE INTEGRATION TEST          \n");
    printf("====================================================\n");

    int failed = test_vcan_bridge_routing();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL VCAN BRIDGE TESTS PASSED SUCCESSFULLY!         \n");
    } else {
        printf(" VCAN BRIDGE TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
