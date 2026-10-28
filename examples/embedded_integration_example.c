/*
 * embedded_integration_example.c â€” Sample Embedded Integration
 *
 * Demonstrates how to initialize MCP2515 over SPI, register frame callbacks,
 * and integrate can-logger-decoder into an embedded RTOS (e.g. FreeRTOS)
 * or bare-metal main loop.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "mcp2515.h"

int main(void) {
    printf("Initializing CAN Logger in embedded mode...\n");

    /* 1. Initialize hardware SPI and MCP2515 */
    mcp2515_t *dev = mcp2515_create(NULL, NULL);
    if (!dev) {
        printf("Failed to allocate driver instance\n");
        return 1;
    }

    if (mcp2515_init(dev, 500000) != CAN_OK) {
        printf("Failed to initialize MCP2515 at 500 kbps\n");
        mcp2515_destroy(dev);
        return 1;
    }

    /* 2. Configure receive filters */
    mcp2515_set_filter(dev, 0, 0x100, false);
    mcp2515_set_mask(dev, 0, 0x7F0, false);

    printf("Embedded CAN interface ready and listening.\n");
    mcp2515_destroy(dev);
    return 0;
}