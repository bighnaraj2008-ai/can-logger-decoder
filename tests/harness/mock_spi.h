#ifndef HARNESS_MOCK_SPI_H
#define HARNESS_MOCK_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void mock_spi_init(void);
void mock_spi_reset(void);
void mock_spi_set_bus_fault(bool enable);
int mock_spi_select(void);
int mock_spi_deselect(void);
uint8_t mock_spi_read_register(uint8_t reg);
void mock_spi_write_register(uint8_t reg, uint8_t val);
void mock_spi_bit_modify(uint8_t reg, uint8_t mask, uint8_t val);
int mock_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len);

#endif /* HARNESS_MOCK_SPI_H */
