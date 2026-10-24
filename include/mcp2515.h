#ifndef MCP2515_H
#define MCP2515_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "can_filters.h"
#include "error_codes.h"

/**
 * @brief MCP2515 stand-alone CAN controller (SPI).
 *
 * This header defines the register map, SPI command opcodes, operating
 * modes and bit-timing configuration constants for the Microchip MCP2515.
 * The actual SPI transport is injected through mcp2515_spi_ops_t so the
 * driver compiles and can be unit-tested on any host (real SPI, software
 * register model, or both).
 */

/* ----------------------------------------------------------------------
 * SPI command opcodes
 * ---------------------------------------------------------------------- */
#define MCP2515_CMD_WRITE          0x02u   /**< Write register */
#define MCP2515_CMD_READ           0x03u   /**< Read register */
#define MCP2515_CMD_BITMODIFY      0x05u   /**< Bit-level register modify */
#define MCP2515_CMD_LOAD_TX0       0x40u   /**< Load transmit buffer 0 */
#define MCP2515_CMD_LOAD_TX1       0x42u   /**< Load transmit buffer 1 */
#define MCP2515_CMD_LOAD_TX2       0x44u   /**< Load transmit buffer 2 */
#define MCP2515_CMD_RTS_TX0        0x81u   /**< Request-to-send buffer 0 */
#define MCP2515_CMD_RTS_TX1        0x82u   /**< Request-to-send buffer 1 */
#define MCP2515_CMD_RTS_TX2        0x84u   /**< Request-to-send buffer 2 */
#define MCP2515_CMD_RTS_ALL        0x87u   /**< Request-to-send all buffers */
#define MCP2515_CMD_READ_STATUS    0xA0u   /**< Read TX/RX status */
#define MCP2515_CMD_RX_STATUS      0xB0u   /**< Read RX filter match status */
#define MCP2515_CMD_RESET          0xC0u   /**< Software reset */

/* ----------------------------------------------------------------------
 * Register map (subset used by the driver)
 * ---------------------------------------------------------------------- */
#define MCP2515_RXF0SIDH           0x00u   /**< RX filter 0 SID high */
#define MCP2515_RXF0SIDL           0x01u   /**< RX filter 0 SID low */
#define MCP2515_RXF0EID8           0x02u   /**< RX filter 0 EID high */
#define MCP2515_RXF0EID0           0x03u   /**< RX filter 0 EID low */
#define MCP2515_RXF1SIDH           0x04u   /**< RX filter 1 SID high */
#define MCP2515_RXF1SIDL           0x05u
#define MCP2515_RXF1EID8           0x06u
#define MCP2515_RXF1EID0           0x07u
#define MCP2515_RXF2SIDH           0x08u   /**< RX filter 2 SID high */
#define MCP2515_RXF2SIDL           0x09u
#define MCP2515_RXF2EID8           0x0Au
#define MCP2515_RXF2EID0           0x0Bu
#define MCP2515_CANSTAT            0x0Eu   /**< CAN status (mode, etc.) */
#define MCP2515_CANCTRL            0x0Fu   /**< CAN control (mode, etc.) */
#define MCP2515_RXM0SIDH           0x20u   /**< RX mask 0 SID high */
#define MCP2515_RXM0SIDL           0x21u
#define MCP2515_RXM0EID8           0x22u
#define MCP2515_RXM0EID0           0x23u
#define MCP2515_RXM1SIDH           0x24u   /**< RX mask 1 SID high */
#define MCP2515_RXM1SIDL           0x25u
#define MCP2515_RXM1EID8           0x26u
#define MCP2515_RXM1EID0           0x27u
#define MCP2515_CNF3               0x28u   /**< Configuration 3 */
#define MCP2515_CNF2               0x29u   /**< Configuration 2 */
#define MCP2515_CNF1               0x2Au   /**< Configuration 1 */
#define MCP2515_CANINTE            0x2Bu   /**< Interrupt enable */
#define MCP2515_CANINTF            0x2Cu   /**< Interrupt flags */
#define MCP2515_EFLG               0x2Du   /**< Error flag */
#define MCP2515_TEC                0x1Cu   /**< Transmit error counter */
#define MCP2515_REC                0x1Du   /**< Receive error counter */
#define MCP2515_RXB0CTRL           0x60u   /**< RX buffer 0 control */
#define MCP2515_RXB1CTRL           0x70u   /**< RX buffer 1 control */
#define MCP2515_TXB0CTRL           0x30u   /**< TX buffer 0 control */

/* ----------------------------------------------------------------------
 * CANCTRL / CANSTAT mode bits
 * ---------------------------------------------------------------------- */
#define MCP2515_CTRL_MODE_MASK     0xE0u   /**< REQOP2:0 / OPMOD2:0 mask */
#define MCP2515_CTRL_ABAT          0x10u   /**< Abort all pending transmissions */
#define MCP2515_CTRL_OSM           0x08u   /**< One-shot mode */
#define MCP2515_CTRL_CLKEN         0x04u   /**< CLKOUT enable */
#define MCP2515_CTRL_CLKPRE        0x01u   /**< CLKOUT prescaler */

#define MCP2515_MODE_NORMAL        0x00u   /**< Normal operating mode */
#define MCP2515_MODE_SLEEP         0x20u   /**< Sleep mode */
#define MCP2515_MODE_LOOPBACK      0x40u   /**< Loopback mode */
#define MCP2515_MODE_LISTEN_ONLY   0x60u   /**< Listen-only mode */
#define MCP2515_MODE_CONFIG        0x80u   /**< Configuration mode */
#define MCP2515_MODE_POWER_UP      0xE0u   /**< Power-up reset default */

/* CANSTAT status bits */
#define MCP2515_STAT_ICODE_MASK    0xE0u   /**< Interrupt code mask */
#define MCP2515_STAT_TX0IF         0x10u   /**< TX buffer 0 empty */
#define MCP2515_STAT_TX1IF         0x08u   /**< TX buffer 1 empty */
#define MCP2515_STAT_TX2IF         0x04u   /**< TX buffer 2 empty */
#define MCP2515_STAT_RX0IF         0x02u   /**< RX buffer 0 full */
#define MCP2515_STAT_RX1IF         0x01u   /**< RX buffer 1 full */

/* ----------------------------------------------------------------------
 * Bit-timing / device constants
 * ---------------------------------------------------------------------- */
#define MCP2515_DEFAULT_OSC_HZ     16000000u /**< Default crystal frequency */
#define MCP2515_TEC_BUS_OFF_LIMIT  256u      /**< TEC > 255 => bus-off (CAN spec) */
#define MCP2515_REG_MODE_POLL_MAX  10000u    /**< Mode-change poll iterations */

/**
 * @brief Bit-timing parameters programmed into CNF1/CNF2/CNF3.
 */
typedef struct {
    uint8_t brp;       /**< Baud rate prescaler (0-63) */
    uint8_t sjw;       /**< Synchronization jump width (1-4) */
    uint8_t prseg;     /**< Propagation time segment (1-8 TQ) */
    uint8_t phseg1;    /**< Phase segment 1 (1-8 TQ) */
    uint8_t phseg2;    /**< Phase segment 2 (2-8 TQ) */
    uint8_t sam;       /**< Sample point (1 = once, 3 = three times) */
} mcp2515_bit_timing_t;

/* Opaque device handle; full definition lives in mcp2515_spi.c */
typedef struct mcp2515 mcp2515_t;

/* ----------------------------------------------------------------------
 * SPI transport callbacks (injected by the application)
 * ---------------------------------------------------------------------- */

/**
 * @brief Read a single register through the SPI transport.
 * @return 0 on success, negative can_error_t code on failure.
 */
typedef int (*mcp2515_spi_read_fn)(void *ctx, uint8_t reg, uint8_t *value);

/**
 * @brief Write a single register through the SPI transport.
 */
typedef int (*mcp2515_spi_write_fn)(void *ctx, uint8_t reg, uint8_t value);

/**
 * @brief Read a burst of consecutive registers (tx/rx buffers).
 */
typedef int (*mcp2515_spi_read_burst_fn)(void *ctx, uint8_t reg,
                                         uint8_t *buf, size_t len);

/**
 * @brief Write a burst of consecutive registers.
 */
typedef int (*mcp2515_spi_write_burst_fn)(void *ctx, uint8_t reg,
                                          const uint8_t *buf, size_t len);

/**
 * @brief SPI transport operation table passed to mcp2515_create().
 *
 * Any callback may be NULL; when the single-register read/write callbacks
 * are absent the driver falls back to an internal software register model
 * that works without any hardware.
 */
typedef struct {
    mcp2515_spi_read_fn        read;
    mcp2515_spi_write_fn       write;
    mcp2515_spi_read_burst_fn  read_burst;
    mcp2515_spi_write_burst_fn write_burst;
    void                       *ctx;
} mcp2515_spi_ops_t;

/* ----------------------------------------------------------------------
 * Device lifecycle
 * ---------------------------------------------------------------------- */

/**
 * @brief Create an MCP2515 device instance.
 *
 * When ops is NULL (or its read/write callbacks are NULL) the device runs
 * in software-model mode: all register accesses operate on an internal
 * cache and no physical SPI traffic occurs.
 *
 * @param dev_out   Receives the allocated device handle
 * @param ops       SPI transport callbacks (may be NULL)
 * @param ctx       Opaque context forwarded to the transport callbacks
 * @return CAN_OK on success, CAN_ERR_NOMEM on allocation failure
 */
int mcp2515_create(mcp2515_t **dev_out, const mcp2515_spi_ops_t *ops, void *ctx);

/**
 * @brief Destroy a device instance created with mcp2515_create().
 */
void mcp2515_destroy(mcp2515_t *dev);

/**
 * @brief Issue the SPI software-reset command and restore default state.
 */
int mcp2515_reset(mcp2515_t *dev);

/**
 * @brief Switch the controller operating mode (polling CANSTAT for ack).
 */
int mcp2515_set_mode(mcp2515_t *dev, uint8_t mode);

/**
 * @brief Read back the current operating mode from CANSTAT.
 */
int mcp2515_get_mode(mcp2515_t *dev, uint8_t *mode);

/**
 * @brief Read a single MCP2515 register.
 */
int mcp2515_read_reg(mcp2515_t *dev, uint8_t reg, uint8_t *value);

/**
 * @brief Write a single MCP2515 register.
 */
int mcp2515_write_reg(mcp2515_t *dev, uint8_t reg, uint8_t value);

/**
 * @brief Read-modify-write a register: reg = (reg & ~mask) | (value & mask).
 */
int mcp2515_bit_modify(mcp2515_t *dev, uint8_t reg, uint8_t mask, uint8_t value);

/**
 * @brief Compute and program CNF1/CNF2/CNF3 for the requested bit rate.
 *
 * Only the default 16 MHz oscillator is supported exactly; other crystal
 * frequencies are rounded to the nearest achievable bit rate.
 */
int mcp2515_set_bitrate(mcp2515_t *dev, uint32_t bitrate_hz);

/**
 * @brief Program the acceptance filter masks and filter registers from a
 *        can_filter_set_t. The controller must be in CONFIG mode.
 *
 * Mapping: filters[0]/mask[0] -> RXF0/RXM0 (RX buffer 0), filters[1]/mask[1]
 * -> RXF1/RXM1 (RX buffer 1). Additional entries are ignored by this model.
 */
int mcp2515_configure_acceptance_filters(mcp2515_t *dev,
                                         const can_filter_set_t *filters);

/**
 * @brief Read the CAN status register (TX/RX buffer status flags).
 */
int mcp2515_read_status(mcp2515_t *dev, uint8_t *status);

/**
 * @brief Read the TEC/REC error counters.
 */
int mcp2515_error_counters(mcp2515_t *dev, uint8_t *tec, uint8_t *rec);

/**
 * @brief Test whether the controller is currently in bus-off (TEC > 255).
 */
int mcp2515_is_bus_off(const mcp2515_t *dev, bool *bus_off);

/**
 * @brief Test helper: directly set TEC/REC counters (software model only).
 * @return CAN_ERR_INTERNAL when a real SPI transport is in use.
 */
int mcp2515_inject_error_counters(mcp2515_t *dev, uint8_t tec, uint8_t rec);

/**
 * @brief Test helper: force the controller mode register (software model only).
 */
int mcp2515_inject_mode(mcp2515_t *dev, uint8_t mode);

#endif /* MCP2515_H */
