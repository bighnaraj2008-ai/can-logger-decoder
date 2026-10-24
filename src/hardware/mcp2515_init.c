#define _GNU_SOURCE
#include "mcp2515.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief Initialize the MCP2515 controller hardware sequence.
 *
 * Performs reset, bit rate timing configuration, RX buffer setup,
 * and enters the requested operational mode.
 *
 * @param dev Active MCP2515 device handle
 * @param bitrate_hz Desired CAN bus bitrate (e.g., 250000, 500000)
 * @param target_mode Operating mode (e.g., MCP2515_MODE_NORMAL)
 * @return CAN_OK on success, or a negative can_error_t code.
 */
int mcp2515_hw_init(mcp2515_t *dev, uint32_t bitrate_hz, uint8_t target_mode) {
    if (dev == NULL) {
        return CAN_ERR_INVALID_ARG;
    }

    /* Step 1: Issue software reset command */
    int rc = mcp2515_reset(dev);
    if (rc != CAN_OK) {
        return rc;
    }

    /* Step 2: Ensure device is in Configuration Mode */
    rc = mcp2515_set_mode(dev, MCP2515_MODE_CONFIG);
    if (rc != CAN_OK) {
        return rc;
    }

    /* Step 3: Program bit rate timings into CNF1, CNF2, CNF3 */
    rc = mcp2515_set_bitrate(dev, bitrate_hz);
    if (rc != CAN_OK) {
        return rc;
    }

    /* Step 4: Configure RX Buffer 0/1 control (rollover enabled, any format) */
    rc = mcp2515_write_reg(dev, MCP2515_RXB0CTRL, 0x64); /* RXM=11 (off), BUKT=1 */
    if (rc != CAN_OK) {
        return rc;
    }
    rc = mcp2515_write_reg(dev, MCP2515_RXB1CTRL, 0x60); /* RXM=11 (off) */
    if (rc != CAN_OK) {
        return rc;
    }

    /* Step 5: Enable RX and Error interrupts (RX0IE, RX1IE, ERRIE) */
    uint8_t inte = 0x01 | 0x02 | 0x20;
    rc = mcp2515_write_reg(dev, MCP2515_CANINTE, inte);
    if (rc != CAN_OK) {
        return rc;
    }

    /* Step 6: Transition into target operating mode */
    rc = mcp2515_set_mode(dev, target_mode);
    if (rc != CAN_OK) {
        return rc;
    }

    return CAN_OK;
}

/**
 * @brief Query the operational status and mode of the MCP2515.
 */
int mcp2515_hw_check_status(mcp2515_t *dev, bool *is_ready) {
    if (dev == NULL || is_ready == NULL) {
        return CAN_ERR_INVALID_ARG;
    }

    uint8_t mode = 0;
    int rc = mcp2515_get_mode(dev, &mode);
    if (rc != CAN_OK) {
        *is_ready = false;
        return rc;
    }

    *is_ready = (mode == MCP2515_MODE_NORMAL ||
                 mode == MCP2515_MODE_LOOPBACK ||
                 mode == MCP2515_MODE_LISTEN_ONLY);
    return CAN_OK;
}
