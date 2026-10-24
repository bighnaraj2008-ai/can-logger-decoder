#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "can_filters.h"
#include "error_codes.h"
#include "mcp2515.h"
#include "vcan_interface.h"

typedef struct {
    mcp2515_t         *hw_dev;
    vcan_socket_t     *vcan_sock;
    can_filter_set_t   filter_set;
    uint64_t           routed_to_vcan;
    uint64_t           routed_to_hw;
    uint64_t           dropped_frames;
} can_router_t;

int can_router_init(can_router_t *router, mcp2515_t *hw_dev, vcan_socket_t *vcan_sock);
int can_router_set_filters(can_router_t *router, const can_filter_set_t *filters);
int can_router_forward_to_vcan(can_router_t *router, const can_frame_t *frame);
int can_router_forward_to_hw(can_router_t *router, const can_frame_t *frame);
void can_router_get_stats(const can_router_t *router, uint64_t *to_vcan, uint64_t *to_hw, uint64_t *drops);

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

static int test_router_forwarding(void) {
    printf("\n=== Running CAN Router Forwarding Tests ===\n");

    can_router_t router;
    int rc = can_router_init(&router, NULL, NULL);
    TEST_ASSERT(rc == CAN_OK, "Router initialized");

    can_frame_t f;
    can_frame_init(&f);
    f.can_id = 0x123;
    f.can_dlc = 8;

    rc = can_router_forward_to_vcan(&router, &f);
    TEST_ASSERT(rc == 1, "Forwarded to vcan in promiscuous mode");

    rc = can_router_forward_to_hw(&router, &f);
    TEST_ASSERT(rc == 1, "Forwarded to hw in promiscuous mode");

    uint64_t to_vcan = 0, to_hw = 0, drops = 0;
    can_router_get_stats(&router, &to_vcan, &to_hw, &drops);
    TEST_ASSERT(to_vcan == 1, "1 frame routed to vcan");
    TEST_ASSERT(to_hw == 1, "1 frame routed to hw");
    TEST_ASSERT(drops == 0, "0 frames dropped");

    /* Apply filter rejecting 0x123 */
    can_filter_set_t fs;
    can_filter_set_init(&fs);
    can_filter_set_add(&fs, 0x456, CAN_SFF_MASK, CAN_FILTER_MODE_STANDARD);
    can_router_set_filters(&router, &fs);

    rc = can_router_forward_to_vcan(&router, &f);
    TEST_ASSERT(rc == 0, "Rejected frame not matching filter");

    can_router_get_stats(&router, &to_vcan, &to_hw, &drops);
    TEST_ASSERT(drops == 1, "Drop counter incremented to 1");

    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" CAN ROUTER UNIT TEST SUITE                         \n");
    printf("====================================================\n");

    int failed = 0;
    failed += test_router_forwarding();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL ROUTER TESTS PASSED SUCCESSFULLY!              \n");
    } else {
        printf(" ROUTER TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
