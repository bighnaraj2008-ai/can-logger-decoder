#define _GNU_SOURCE
#include "can_core.h"
#include "error_codes.h"

#include <stdio.h>

/**
 * @brief Strict validation of Data Length Code (DLC).
 *
 * For classical CAN, valid DLC is between 0 and 8 inclusive.
 */
bool can_dlc_is_valid(uint8_t dlc) {
    return dlc <= CAN_DLC_MAX;
}

/**
 * @brief Convert DLC to byte length.
 */
int can_dlc_to_bytes(uint8_t dlc) {
    if (!can_dlc_is_valid(dlc)) {
        return CAN_ERR_INVALID_DLC;
    }
    return (int)dlc;
}

/**
 * @brief Convert payload byte count to DLC.
 */
int can_bytes_to_dlc(size_t length) {
    if (length > CAN_DLC_MAX) {
        return -1;
    }
    return (int)length;
}

/**
 * @brief Map DLC to payload byte length, including ISO 15765-2 CAN FD mappings.
 */
uint8_t can_dlc_to_length(uint8_t dlc) {
    static const uint8_t fd_lengths[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, /* 0 - 8 */
        12, 16, 20, 24, 32, 48, 64  /* 9 - 15 */
    };
    if (dlc <= 15) {
        return fd_lengths[dlc];
    }
    return CAN_DLC_MAX;
}

/**
 * @brief Check if raw CAN identifier carries the 29-bit extended flag.
 */
bool can_id_is_extended(uint32_t raw_id) {
    return (raw_id & CAN_EFF_FLAG) != 0u;
}

/**
 * @brief Strip EFF, RTR, ERR controller flag bits from identifier.
 */
uint32_t can_id_clean(uint32_t raw_id) {
    if (can_id_is_extended(raw_id)) {
        return raw_id & CAN_EFF_MASK;
    }
    return raw_id & CAN_SFF_MASK;
}

/**
 * @brief Return semantic version string of the core module.
 */
const char *can_core_version(void) {
    static char vbuf[32];
    snprintf(vbuf, sizeof(vbuf), "%u.%u.%u",
             CAN_CORE_VERSION_MAJOR,
             CAN_CORE_VERSION_MINOR,
             CAN_CORE_VERSION_PATCH);
    return vbuf;
}
