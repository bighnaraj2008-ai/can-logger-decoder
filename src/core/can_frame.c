#define _GNU_SOURCE
#include "can_core.h"
#include "error_codes.h"

#include <string.h>

/* ----------------------------------------------------------------------
 * CRC-8 (polynomial 0x07, init 0x00, no reflection)
 *
 * Used for payload checksumming on mock frames and by any higher layer
 * that wants end-to-end payload integrity checks.
 * ---------------------------------------------------------------------- */
static uint8_t crc8_update(uint8_t crc, uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    }
    return crc;
}

uint8_t can_checksum_compute(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00u;
    if (data != NULL) {
        for (size_t i = 0; i < len; i++) {
            crc = crc8_update(crc, data[i]);
        }
    }
    return crc;
}

void can_checksum_append(uint8_t *data, size_t len) {
    if (data == NULL) return;
    data[len] = can_checksum_compute(data, len);
}

bool can_checksum_validate(const uint8_t *data, size_t len, uint8_t checksum) {
    if (data == NULL) return false;
    return can_checksum_compute(data, len) == checksum;
}

/* ----------------------------------------------------------------------
 * Frame lifecycle
 * ---------------------------------------------------------------------- */
void can_frame_init(can_frame_t *frame) {
    if (frame == NULL) return;
    memset(frame, 0, sizeof(*frame));
    frame->can_dlc = CAN_DLC_MIN;
    frame->checksum_valid = false;
}

void can_msg_clear(can_msg_t *msg) {
    if (msg == NULL) return;
    memset(msg, 0, sizeof(*msg));
    msg->dlc = CAN_DLC_MIN;
    msg->checksum_valid = false;
}

bool can_frame_is_valid(const can_frame_t *frame) {
    if (frame == NULL) return false;
    return can_dlc_is_valid(frame->can_dlc);
}

/* ----------------------------------------------------------------------
 * Wire format: [can_id_31..24][can_id_23..16][can_id_15..8][can_id_7..0]
 *              [can_dlc][data byte 0]...[data byte N-1]
 * ---------------------------------------------------------------------- */
can_error_t can_frame_to_wire(const can_frame_t *frame, uint8_t *wire, size_t *wire_len) {
    if (frame == NULL || wire == NULL || wire_len == NULL) {
        return CAN_ERR_INVALID_ARG;
    }
    if (!can_dlc_is_valid(frame->can_dlc)) {
        return CAN_ERR_INVALID_DLC;
    }

    size_t needed = 4u + 1u + (size_t)frame->can_dlc;
    if (*wire_len < needed) {
        *wire_len = needed;
        return CAN_ERR_IO; /* buffer too small (reported as IO shortfall) */
    }

    wire[0] = (uint8_t)(frame->can_id >> 24);
    wire[1] = (uint8_t)(frame->can_id >> 16);
    wire[2] = (uint8_t)(frame->can_id >> 8);
    wire[3] = (uint8_t)(frame->can_id);
    wire[4] = frame->can_dlc;
    if (frame->can_dlc > 0) {
        memcpy(wire + 5, frame->data, (size_t)frame->can_dlc);
    }
    *wire_len = needed;
    return CAN_OK;
}

can_error_t can_frame_from_wire(const uint8_t *wire, size_t wire_len,
                                can_frame_t *frame) {
    if (wire == NULL || frame == NULL) {
        return CAN_ERR_INVALID_ARG;
    }
    if (wire_len < 5u) {
        return CAN_ERR_IO; /* truncated frame */
    }

    can_frame_init(frame);
    frame->can_id = ((uint32_t)wire[0] << 24) | ((uint32_t)wire[1] << 16) |
                    ((uint32_t)wire[2] << 8) | (uint32_t)wire[3];
    frame->can_dlc = wire[4];

    if (!can_dlc_is_valid(frame->can_dlc)) {
        return CAN_ERR_INVALID_DLC;
    }
    if (wire_len < 5u + (size_t)frame->can_dlc) {
        return CAN_ERR_IO; /* truncated payload */
    }

    if (frame->can_dlc > 0) {
        memcpy(frame->data, wire + 5, (size_t)frame->can_dlc);
    }

    frame->is_extended = can_id_is_extended(frame->can_id);
    frame->is_rtr = ((frame->can_id & CAN_RTR_FLAG) != 0u);
    frame->is_error = ((frame->can_id & CAN_ERR_FLAG) != 0u);
    return CAN_OK;
}

void can_frame_copy(can_frame_t *dst, const can_frame_t *src) {
    if (dst == NULL || src == NULL) return;
    *dst = *src;
}
