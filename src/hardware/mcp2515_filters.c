#define _GNU_SOURCE
#include "mcp2515.h"
#include "can_filters.h"
#include "can_core.h"
#include "error_codes.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief Split a 32-bit CAN identifier/mask into MCP2515 register bytes:
 *        [SIDH, SIDL, EID8, EID0].
 */
static void can_id_to_mcp_registers(uint32_t id, bool is_extended, uint8_t regs[4]) {
    if (is_extended) {
        /* 29-bit identifier */
        regs[0] = (uint8_t)((id >> 21) & 0xFFu);            /* SIDH: ID<28:21> */
        regs[1] = (uint8_t)(((id >> 13) & 0xE0u) | 0x08u |  /* EXIDE bit set */
                            ((id >> 16) & 0x03u));          /* SIDL: ID<20:18>, EXIDE, EID<17:16> */
        regs[2] = (uint8_t)((id >> 8) & 0xFFu);             /* EID8: EID<15:8> */
        regs[3] = (uint8_t)(id & 0xFFu);                    /* EID0: EID<7:0> */
    } else {
        /* 11-bit identifier */
        regs[0] = (uint8_t)((id >> 3) & 0xFFu);             /* SIDH: ID<10:3> */
        regs[1] = (uint8_t)((id << 5) & 0xE0u);             /* SIDL: ID<2:0>, EXIDE=0 */
        regs[2] = 0x00u;
        regs[3] = 0x00u;
    }
}

/**
 * @brief Write acceptance filter register values for a specified filter index (0..5).
 */
int mcp2515_set_filter_registers(mcp2515_t *dev, uint8_t filter_idx, uint32_t id, bool is_extended) {
    if (dev == NULL || filter_idx > 5) {
        return CAN_ERR_INVALID_ARG;
    }

    static const uint8_t filter_bases[] = {
        MCP2515_RXF0SIDH, MCP2515_RXF1SIDH, MCP2515_RXF2SIDH,
        0x10u, 0x14u, 0x18u /* RXF3, RXF4, RXF5 bases */
    };

    uint8_t regs[4];
    can_id_to_mcp_registers(id, is_extended, regs);

    uint8_t base = filter_bases[filter_idx];
    for (uint8_t i = 0; i < 4; i++) {
        int rc = mcp2515_write_reg(dev, (uint8_t)(base + i), regs[i]);
        if (rc != CAN_OK) return rc;
    }
    return CAN_OK;
}

/**
 * @brief Write acceptance mask register values for RXM0 or RXM1.
 */
int mcp2515_set_mask_registers(mcp2515_t *dev, uint8_t mask_idx, uint32_t mask, bool is_extended) {
    if (dev == NULL || mask_idx > 1) {
        return CAN_ERR_INVALID_ARG;
    }

    uint8_t base = (mask_idx == 0) ? MCP2515_RXM0SIDH : MCP2515_RXM1SIDH;
    uint8_t regs[4];
    can_id_to_mcp_registers(mask, is_extended, regs);

    for (uint8_t i = 0; i < 4; i++) {
        int rc = mcp2515_write_reg(dev, (uint8_t)(base + i), regs[i]);
        if (rc != CAN_OK) return rc;
    }
    return CAN_OK;
}

/**
 * @brief Configure MCP2515 acceptance filters from high-level filter set.
 */
int mcp2515_configure_acceptance_filters(mcp2515_t *dev, const can_filter_set_t *filters) {
    if (dev == NULL) return CAN_ERR_INVALID_ARG;
    if (filters == NULL || filters->promiscuous || filters->count == 0) {
        /* Disable filtering: clear masks to 0 */
        mcp2515_set_mask_registers(dev, 0, 0x00000000u, false);
        mcp2515_set_mask_registers(dev, 1, 0x00000000u, false);
        return CAN_OK;
    }

    /* Program filter 0 and mask 0 */
    bool ext0 = (filters->filters[0].mode == CAN_FILTER_MODE_EXTENDED);
    mcp2515_set_filter_registers(dev, 0, filters->filters[0].id, ext0);
    mcp2515_set_mask_registers(dev, 0, filters->filters[0].mask, ext0);

    /* Program filter 1 and mask 1 if present */
    if (filters->count > 1) {
        bool ext1 = (filters->filters[1].mode == CAN_FILTER_MODE_EXTENDED);
        mcp2515_set_filter_registers(dev, 1, filters->filters[1].id, ext1);
        mcp2515_set_mask_registers(dev, 1, filters->filters[1].mask, ext1);
    }

    return CAN_OK;
}

/* ----------------------------------------------------------------------
 * High-level Acceptance Filter Functions
 * ---------------------------------------------------------------------- */
void can_filter_set_init(can_filter_set_t *set) {
    if (set == NULL) return;
    memset(set, 0, sizeof(*set));
    set->promiscuous = false;
}

void can_filter_set_add(can_filter_set_t *set,
                        uint32_t id, uint32_t mask, can_filter_mode_t mode) {
    if (set == NULL) return;
    if (set->count >= CAN_FILTER_COUNT_MAX) return;
    can_filter_t *f = &set->filters[set->count++];
    f->id = id;
    f->mask = mask;
    f->mode = mode;
    f->enabled = true;
}

void can_filter_set_clear(can_filter_set_t *set) {
    if (set == NULL) return;
    memset(set->filters, 0, sizeof(set->filters));
    set->count = 0;
    set->promiscuous = false;
}

bool can_filter_match(const can_filter_set_t *set, const can_frame_t *frame) {
    if (set == NULL || frame == NULL) return false;
    if (set->promiscuous) return true;

    uint32_t fid = can_id_clean(frame->can_id);
    bool frame_ext = can_id_is_extended(frame->can_id);

    for (size_t i = 0; i < set->count; i++) {
        const can_filter_t *f = &set->filters[i];
        if (!f->enabled) continue;

        bool format_ok;
        if (f->mode == CAN_FILTER_MODE_DUAL) {
            format_ok = true;
        } else {
            format_ok = (f->mode == CAN_FILTER_MODE_EXTENDED) == frame_ext;
        }
        if (!format_ok) continue;

        uint32_t frame_bits = frame_ext ? fid : (fid & CAN_SFF_MASK);
        uint32_t filter_bits = f->id & f->mask & (frame_ext ? CAN_EFF_MASK : CAN_SFF_MASK);
        uint32_t mask_bits = f->mask & (frame_ext ? CAN_EFF_MASK : CAN_SFF_MASK);

        if ((frame_bits & mask_bits) == (filter_bits & mask_bits)) {
            return true;
        }
    }
    return false;
}

int can_filter_route(const can_filter_set_t *set, const can_frame_t *frame) {
    if (set == NULL || frame == NULL) return -1;
    if (set->promiscuous) return 0;

    uint32_t fid = can_id_clean(frame->can_id);
    bool frame_ext = can_id_is_extended(frame->can_id);

    for (size_t i = 0; i < set->count; i++) {
        const can_filter_t *f = &set->filters[i];
        if (!f->enabled) continue;

        bool format_ok;
        if (f->mode == CAN_FILTER_MODE_DUAL) {
            format_ok = true;
        } else {
            format_ok = (f->mode == CAN_FILTER_MODE_EXTENDED) == frame_ext;
        }
        if (!format_ok) continue;

        uint32_t frame_bits = frame_ext ? fid : (fid & CAN_SFF_MASK);
        uint32_t filter_bits = f->id & f->mask & (frame_ext ? CAN_EFF_MASK : CAN_SFF_MASK);
        uint32_t mask_bits = f->mask & (frame_ext ? CAN_EFF_MASK : CAN_SFF_MASK);

        if ((frame_bits & mask_bits) == (filter_bits & mask_bits)) {
            return (int)i;
        }
    }
    return -1;
}

int vcan_route_frame(const can_frame_t *frame, const can_filter_set_t *filters) {
    if (frame == NULL || filters == NULL) return -1;
    return can_filter_route(filters, frame);
}
