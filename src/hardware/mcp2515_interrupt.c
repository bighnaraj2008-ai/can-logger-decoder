#define _GNU_SOURCE
#include "mcp2515.h"

#include <stdio.h>
#include <string.h>

/* MCP2515 CANINTF Interrupt Flag Bits */
#define MCP2515_INTF_RX0IF  0x01u
#define MCP2515_INTF_RX1IF  0x02u
#define MCP2515_INTF_TX0IF  0x04u
#define MCP2515_INTF_TX1IF  0x08u
#define MCP2515_INTF_TX2IF  0x10u
#define MCP2515_INTF_ERRIF  0x20u
#define MCP2515_INTF_WAKIF  0x40u
#define MCP2515_INTF_MERRF  0x80u

/**
 * @brief Read the active interrupt flags from CANINTF.
 */
int mcp2515_get_interrupt_flags(mcp2515_t *dev, uint8_t *flags) {
    if (dev == NULL || flags == NULL) {
        return CAN_ERR_INVALID_ARG;
    }
    return mcp2515_read_reg(dev, MCP2515_CANINTF, flags);
}

/**
 * @brief Clear specified interrupt flags in CANINTF.
 */
int mcp2515_clear_interrupt_flags(mcp2515_t *dev, uint8_t mask) {
    if (dev == NULL) {
        return CAN_ERR_INVALID_ARG;
    }
    /* Write 0 to the bits to be cleared */
    return mcp2515_bit_modify(dev, MCP2515_CANINTF, mask, 0x00);
}

/**
 * @brief Refined interrupt service dispatcher for SPI events.
 *
 * Checks pending interrupt flags, dispatches status events, and clears flags.
 *
 * @param dev Active MCP2515 device handle
 * @param rx_pending Out: true if RX buffer contains frames
 * @param tx_ready Out: true if a TX buffer is available
 * @param err_detected Out: true if bus/controller error occurred
 * @return CAN_OK on success, or a negative can_error_t code.
 */
int mcp2515_service_interrupts(mcp2515_t *dev, bool *rx_pending, bool *tx_ready, bool *err_detected) {
    if (dev == NULL) {
        return CAN_ERR_INVALID_ARG;
    }

    uint8_t flags = 0;
    int rc = mcp2515_get_interrupt_flags(dev, &flags);
    if (rc != CAN_OK) {
        return rc;
    }

    if (rx_pending != NULL) {
        *rx_pending = (flags & (MCP2515_INTF_RX0IF | MCP2515_INTF_RX1IF)) != 0;
    }
    if (tx_ready != NULL) {
        *tx_ready = (flags & (MCP2515_INTF_TX0IF | MCP2515_INTF_TX1IF | MCP2515_INTF_TX2IF)) != 0;
    }
    if (err_detected != NULL) {
        *err_detected = (flags & (MCP2515_INTF_ERRIF | MCP2515_INTF_MERRF)) != 0;
    }

    /* Acknowledge error and wake flags directly */
    uint8_t to_clear = flags & (MCP2515_INTF_ERRIF | MCP2515_INTF_MERRF | MCP2515_INTF_WAKIF);
    if (to_clear != 0) {
        rc = mcp2515_clear_interrupt_flags(dev, to_clear);
        if (rc != CAN_OK) {
            return rc;
        }
    }

    return CAN_OK;
}
