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

/**
 * @brief Bidirectional CAN Frame Router between hardware SPI and virtual socket.
 */
typedef struct {
    mcp2515_t         *hw_dev;
    vcan_socket_t     *vcan_sock;
    can_filter_set_t   filter_set;
    uint64_t           routed_to_vcan;
    uint64_t           routed_to_hw;
    uint64_t           dropped_frames;
} can_router_t;

int can_router_init(can_router_t *router, mcp2515_t *hw_dev, vcan_socket_t *vcan_sock) {
    if (router == NULL) return CAN_ERR_INVALID_ARG;
    memset(router, 0, sizeof(*router));
    router->hw_dev = hw_dev;
    router->vcan_sock = vcan_sock;
    can_filter_set_init(&router->filter_set);
    router->filter_set.promiscuous = true;
    return CAN_OK;
}

int can_router_set_filters(can_router_t *router, const can_filter_set_t *filters) {
    if (router == NULL || filters == NULL) return CAN_ERR_INVALID_ARG;
    router->filter_set = *filters;
    return CAN_OK;
}

int can_router_forward_to_vcan(can_router_t *router, const can_frame_t *frame) {
    if (router == NULL || frame == NULL) return CAN_ERR_INVALID_ARG;

    /* Check filter rules */
    if (!can_filter_match(&router->filter_set, frame)) {
        router->dropped_frames++;
        return 0; /* Filtered out */
    }

    if (router->vcan_sock != NULL) {
        int rc = vcan_socket_write_frame(router->vcan_sock, frame);
        if (rc > 0) {
            router->routed_to_vcan++;
            return 1;
        }
        return rc;
    }

    router->routed_to_vcan++;
    return 1;
}

int can_router_forward_to_hw(can_router_t *router, const can_frame_t *frame) {
    if (router == NULL || frame == NULL) return CAN_ERR_INVALID_ARG;

    if (!can_filter_match(&router->filter_set, frame)) {
        router->dropped_frames++;
        return 0; /* Filtered out */
    }

    router->routed_to_hw++;
    return 1;
}

void can_router_get_stats(const can_router_t *router, uint64_t *to_vcan, uint64_t *to_hw, uint64_t *drops) {
    if (router == NULL) return;
    if (to_vcan) *to_vcan = router->routed_to_vcan;
    if (to_hw) *to_hw = router->routed_to_hw;
    if (drops) *drops = router->dropped_frames;
}
