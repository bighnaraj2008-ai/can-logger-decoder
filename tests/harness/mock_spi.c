#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"

#define MOCK_SPI_REG_COUNT 256

typedef struct {
    uint8_t registers[MOCK_SPI_REG_COUNT];
    bool is_initialized;
    bool cs_asserted;
    uint32_t transfer_count;
    bool inject_bus_fault;
} mock_spi_driver_t;

static mock_spi_driver_t g_mock_spi;

void mock_spi_init(void) {
    memset(&g_mock_spi, 0, sizeof(g_mock_spi));
    g_mock_spi.is_initialized = true;
    /* Initialize default reset register states */
    g_mock_spi.registers[0x0E] = 0x80; /* CANSTAT: Configuration mode by default */
}

void mock_spi_reset(void) {
    mock_spi_init();
}

void mock_spi_set_bus_fault(bool enable) {
    g_mock_spi.inject_bus_fault = enable;
}

int mock_spi_select(void) {
    if (!g_mock_spi.is_initialized || g_mock_spi.inject_bus_fault) {
        return -1;
    }
    g_mock_spi.cs_asserted = true;
    return 0;
}

int mock_spi_deselect(void) {
    if (!g_mock_spi.is_initialized) {
        return -1;
    }
    g_mock_spi.cs_asserted = false;
    return 0;
}

uint8_t mock_spi_read_register(uint8_t reg) {
    if (g_mock_spi.inject_bus_fault) {
        return 0xFF;
    }
    return g_mock_spi.registers[reg];
}

void mock_spi_write_register(uint8_t reg, uint8_t val) {
    if (!g_mock_spi.inject_bus_fault) {
        g_mock_spi.registers[reg] = val;
    }
}

void mock_spi_bit_modify(uint8_t reg, uint8_t mask, uint8_t val) {
    if (!g_mock_spi.inject_bus_fault) {
        uint8_t current = g_mock_spi.registers[reg];
        g_mock_spi.registers[reg] = (current & ~mask) | (val & mask);
    }
}

int mock_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len) {
    if (!g_mock_spi.is_initialized || g_mock_spi.inject_bus_fault) {
        return -1;
    }
    g_mock_spi.transfer_count++;

    if (len >= 2 && tx != NULL) {
        uint8_t opcode = tx[0];
        uint8_t addr = tx[1];

        if (opcode == 0x03) { /* READ */
            if (rx != NULL) {
                for (size_t i = 2; i < len; i++) {
                    rx[i] = g_mock_spi.registers[(addr + i - 2) % MOCK_SPI_REG_COUNT];
                }
            }
        } else if (opcode == 0x02) { /* WRITE */
            for (size_t i = 2; i < len; i++) {
                g_mock_spi.registers[(addr + i - 2) % MOCK_SPI_REG_COUNT] = tx[i];
            }
        }
    }
    return (int)len;
}
