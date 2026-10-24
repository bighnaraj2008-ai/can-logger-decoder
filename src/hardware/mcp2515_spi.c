#define _GNU_SOURCE
#include "mcp2515.h"

#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------------
 * Internal device instance
 * ---------------------------------------------------------------------- */
struct mcp2515 {
    mcp2515_spi_ops_t ops;
    void             *ops_ctx;

    /* Software register model (used when no real SPI transport is bound) */
    uint8_t reg[256];
    uint8_t mode;
    uint32_t bitrate_hz;
    bool    initialized;

    /* Error counters mirrored from the controller */
    uint8_t tec;
    uint8_t rec;
};

/* ----------------------------------------------------------------------
 * Default (software) SPI transport
 *
 * When the application supplies no transport callbacks, register accesses
 * operate on the internal cache so the driver is fully functional on any
 * host and easy to unit test.
 * ---------------------------------------------------------------------- */
static int soft_spi_read(void *ctx, uint8_t reg, uint8_t *value) {
    mcp2515_t *dev = (mcp2515_t *)ctx;
    if (value == NULL) return CAN_ERR_INVALID_ARG;
    *value = dev->reg[reg];
    return CAN_OK;
}

static int soft_spi_write(void *ctx, uint8_t reg, uint8_t value) {
    mcp2515_t *dev = (mcp2515_t *)ctx;
    dev->reg[reg] = value;
    return CAN_OK;
}

static int soft_spi_read_burst(void *ctx, uint8_t reg, uint8_t *buf, size_t len) {
    mcp2515_t *dev = (mcp2515_t *)ctx;
    if (buf == NULL) return CAN_ERR_INVALID_ARG;
    memcpy(buf, dev->reg + reg, len);
    return CAN_OK;
}

static int soft_spi_write_burst(void *ctx, uint8_t reg, const uint8_t *buf, size_t len) {
    mcp2515_t *dev = (mcp2515_t *)ctx;
    if (buf == NULL) return CAN_ERR_INVALID_ARG;
    memcpy(dev->reg + reg, buf, len);
    return CAN_OK;
}

/* ----------------------------------------------------------------------
 * Device lifecycle
 * ---------------------------------------------------------------------- */
int mcp2515_create(mcp2515_t **dev_out, const mcp2515_spi_ops_t *ops, void *ctx) {
    if (dev_out == NULL) return CAN_ERR_INVALID_ARG;

    mcp2515_t *dev = (mcp2515_t *)calloc(1, sizeof(mcp2515_t));
    if (dev == NULL) return CAN_ERR_NOMEM;

    dev->ops_ctx = ctx;
    if (ops != NULL) {
        dev->ops = *ops;
    } else {
        memset(&dev->ops, 0, sizeof(dev->ops));
    }

    /* Fall back to the software register model when callbacks are absent */
    if (dev->ops.read == NULL) {
        dev->ops.read = soft_spi_read;
        dev->ops_ctx = dev;
    }
    if (dev->ops.write == NULL) {
        dev->ops.write = soft_spi_write;
        dev->ops_ctx = dev;
    }
    if (dev->ops.read_burst == NULL) dev->ops.read_burst = soft_spi_read_burst;
    if (dev->ops.write_burst == NULL) dev->ops.write_burst = soft_spi_write_burst;

    dev->mode = MCP2515_MODE_CONFIG;
    dev->initialized = false;
    *dev_out = dev;
    return CAN_OK;
}

void mcp2515_destroy(mcp2515_t *dev) {
    if (dev != NULL) free(dev);
}

int mcp2515_reset(mcp2515_t *dev) {
    if (dev == NULL) return CAN_ERR_INVALID_ARG;

    /* SPI software-reset command */
    int rc = dev->ops.write(dev->ops_ctx, MCP2515_CANCTRL, MCP2515_MODE_CONFIG);
    if (rc != CAN_OK) return rc;

    /* Restore default register values */
    memset(dev->reg, 0, sizeof(dev->reg));
    dev->reg[MCP2515_CANCTRL] = MCP2515_MODE_CONFIG;
    dev->reg[MCP2515_CANSTAT] = MCP2515_MODE_CONFIG;
    dev->reg[MCP2515_CANINTE] = 0x00u;
    dev->reg[MCP2515_CANINTF] = 0x00u;
    dev->reg[MCP2515_EFLG] = 0x00u;
    dev->reg[MCP2515_TEC] = 0x00u;
    dev->reg[MCP2515_REC] = 0x00u;
    dev->reg[MCP2515_RXB0CTRL] = 0x00u;
    dev->reg[MCP2515_RXB1CTRL] = 0x00u;
    dev->reg[MCP2515_TXB0CTRL] = 0x00u;

    dev->mode = MCP2515_MODE_CONFIG;
    dev->tec = 0;
    dev->rec = 0;
    dev->initialized = true;
    return CAN_OK;
}

int mcp2515_set_mode(mcp2515_t *dev, uint8_t mode) {
    if (dev == NULL) return CAN_ERR_INVALID_ARG;

    uint8_t ctrl = 0;
    int rc = dev->ops.read(dev->ops_ctx, MCP2515_CANCTRL, &ctrl);
    if (rc != CAN_OK) return rc;

    ctrl = (uint8_t)((ctrl & (uint8_t)~MCP2515_CTRL_MODE_MASK) | (mode & MCP2515_CTRL_MODE_MASK));
    rc = dev->ops.write(dev->ops_ctx, MCP2515_CANCTRL, ctrl);
    if (rc != CAN_OK) return rc;

    /* Poll CANSTAT until the controller acknowledges the new mode */
    for (unsigned i = 0; i < MCP2515_REG_MODE_POLL_MAX; i++) {
        uint8_t stat = 0;
        rc = dev->ops.read(dev->ops_ctx, MCP2515_CANSTAT, &stat);
        if (rc != CAN_OK) return rc;
        if ((stat & MCP2515_CTRL_MODE_MASK) == (mode & MCP2515_CTRL_MODE_MASK)) {
            dev->mode = mode;
            return CAN_OK;
        }
    }
    return CAN_ERR_TIMEOUT;
}

int mcp2515_get_mode(mcp2515_t *dev, uint8_t *mode) {
    if (dev == NULL || mode == NULL) return CAN_ERR_INVALID_ARG;
    return dev->ops.read(dev->ops_ctx, MCP2515_CANSTAT, mode);
}

int mcp2515_read_reg(mcp2515_t *dev, uint8_t reg, uint8_t *value) {
    if (dev == NULL || value == NULL) return CAN_ERR_INVALID_ARG;
    return dev->ops.read(dev->ops_ctx, reg, value);
}

int mcp2515_write_reg(mcp2515_t *dev, uint8_t reg, uint8_t value) {
    if (dev == NULL) return CAN_ERR_INVALID_ARG;
    return dev->ops.write(dev->ops_ctx, reg, value);
}

int mcp2515_bit_modify(mcp2515_t *dev, uint8_t reg, uint8_t mask, uint8_t value) {
    if (dev == NULL) return CAN_ERR_INVALID_ARG;

    uint8_t current = 0;
    int rc = dev->ops.read(dev->ops_ctx, reg, &current);
    if (rc != CAN_OK) return rc;

    uint8_t updated = (uint8_t)((current & (uint8_t)~mask) | (value & mask));
    return dev->ops.write(dev->ops_ctx, reg, updated);
}

int mcp2515_read_status(mcp2515_t *dev, uint8_t *status) {
    if (dev == NULL || status == NULL) return CAN_ERR_INVALID_ARG;
    return dev->ops.read(dev->ops_ctx, MCP2515_CANINTF, status);
}

int mcp2515_error_counters(mcp2515_t *dev, uint8_t *tec, uint8_t *rec) {
    if (dev == NULL) return CAN_ERR_INVALID_ARG;
    int rc = dev->ops.read(dev->ops_ctx, MCP2515_TEC, &dev->tec);
    if (rc != CAN_OK) return rc;
    rc = dev->ops.read(dev->ops_ctx, MCP2515_REC, &dev->rec);
    if (rc != CAN_OK) return rc;
    if (tec != NULL) *tec = dev->tec;
    if (rec != NULL) *rec = dev->rec;
    return CAN_OK;
}

int mcp2515_is_bus_off(const mcp2515_t *dev, bool *bus_off) {
    if (dev == NULL || bus_off == NULL) return CAN_ERR_INVALID_ARG;
    uint8_t tec = 0, rec = 0;
    /* const_cast through a temporary read is acceptable for the query */
    int rc = mcp2515_error_counters((mcp2515_t *)dev, &tec, &rec);
    if (rc != CAN_OK) return rc;
    *bus_off = (tec >= (uint8_t)(MCP2515_TEC_BUS_OFF_LIMIT - 1u));
    return CAN_OK;
}

int mcp2515_inject_error_counters(mcp2515_t *dev, uint8_t tec, uint8_t rec) {
    if (dev == NULL) return CAN_ERR_INVALID_ARG;
    if (dev->ops.read != soft_spi_read) return CAN_ERR_INTERNAL;
    dev->tec = tec;
    dev->rec = rec;
    dev->reg[MCP2515_TEC] = tec;
    dev->reg[MCP2515_REC] = rec;
    return CAN_OK;
}

int mcp2515_inject_mode(mcp2515_t *dev, uint8_t mode) {
    if (dev == NULL) return CAN_ERR_INVALID_ARG;
    if (dev->ops.read != soft_spi_read) return CAN_ERR_INTERNAL;
    dev->mode = mode;
    dev->reg[MCP2515_CANSTAT] = (uint8_t)(dev->reg[MCP2515_CANSTAT] & 0x1Fu) | mode;
    return CAN_OK;
}

/* ----------------------------------------------------------------------
 * Bit-timing configuration (CNF1/CNF2/CNF3)
 *
 * Total time quanta per bit = 1 (sync) + PRSEG + PHSEG1 + PHSEG2.
 * TQ = 2 * (BRP + 1) / F_osc. The search below picks the prescaler that
 * reproduces the requested bit rate exactly (or as close as possible)
 * and then splits the remaining quanta into valid segments.
 * ---------------------------------------------------------------------- */
static int mcp2515_compute_timing(uint32_t osc_hz, uint32_t bitrate_hz,
                                  mcp2515_bit_timing_t *bt) {
    if (bt == NULL || osc_hz == 0 || bitrate_hz == 0) {
        return CAN_ERR_INVALID_ARG;
    }

    int best_brp = -1;
    int best_tq = -1;

    for (int brp = 0; brp <= 63; brp++) {
        /* tq_per_bit = Fosc / (2 * (BRP+1) * bitrate) */
        uint64_t denom = 2ULL * (uint64_t)(brp + 1) * (uint64_t)bitrate_hz;
        if (denom == 0 || denom > UINT32_MAX) continue;
        uint32_t d = (uint32_t)denom;
        if (osc_hz % d != 0) continue; /* non-integer time quanta */

        uint32_t tq = osc_hz / d;
        if (tq < 5u || tq > 25u) continue;

        best_brp = brp;
        best_tq = (int)tq;
        break; /* exact match found */
    }

    if (best_brp < 0) return CAN_ERR_IO; /* unachievable bit rate */

    int tq = (best_tq > 0) ? best_tq : 8; /* sensible fallback */
    int remaining = tq - 1; /* subtract SYNC_SEG */

    /* Split remaining quanta: PRSEG, PHSEG1, PHSEG2 (each 1-8), SJW 1-4 */
    int prseg = 1 + remaining / 3;
    int phseg1 = 1 + (remaining - prseg) / 2;
    int phseg2 = remaining - prseg - phseg1 + 1;

    /* Clamp into legal ranges */
    if (prseg < 1) prseg = 1;
    if (prseg > 8) prseg = 8;
    if (phseg1 < 1) phseg1 = 1;
    if (phseg1 > 8) phseg1 = 8;
    if (phseg2 < 2) phseg2 = 2;
    if (phseg2 > 8) phseg2 = 8;
    int sjw = phseg2 < 4 ? phseg2 : 4;

    bt->brp = (uint8_t)best_brp;
    bt->sjw = (uint8_t)sjw;
    bt->prseg = (uint8_t)prseg;
    bt->phseg1 = (uint8_t)phseg1;
    bt->phseg2 = (uint8_t)phseg2;
    bt->sam = 1u;
    return CAN_OK;
}

int mcp2515_set_bitrate(mcp2515_t *dev, uint32_t bitrate_hz) {
    if (dev == NULL || bitrate_hz == 0) return CAN_ERR_INVALID_ARG;

    mcp2515_bit_timing_t bt;
    int rc = mcp2515_compute_timing(MCP2515_DEFAULT_OSC_HZ, bitrate_hz, &bt);
    if (rc != CAN_OK) return rc;

    /* Must be in CONFIG mode to change CNF registers */
    uint8_t mode = 0;
    mcp2515_get_mode(dev, &mode);
    if (mode != MCP2515_MODE_CONFIG) {
        rc = mcp2515_set_mode(dev, MCP2515_MODE_CONFIG);
        if (rc != CAN_OK) return rc;
    }

    uint8_t cnf1 = (uint8_t)(((bt.sjw - 1u) << 6) | bt.brp);
    uint8_t cnf2 = (uint8_t)(((bt.sam & 1u) << 6) |
                             (((bt.phseg1 - 1u) & 7u) << 3) |
                             ((bt.prseg - 1u) & 7u));
    uint8_t cnf3 = (uint8_t)((bt.phseg2 - 1u) & 7u);

    rc = dev->ops.write(dev->ops_ctx, MCP2515_CNF1, cnf1);
    if (rc != CAN_OK) return rc;
    rc = dev->ops.write(dev->ops_ctx, MCP2515_CNF2, cnf2);
    if (rc != CAN_OK) return rc;
    rc = dev->ops.write(dev->ops_ctx, MCP2515_CNF3, cnf3);
    if (rc != CAN_OK) return rc;

    dev->bitrate_hz = bitrate_hz;
    return CAN_OK;
}




