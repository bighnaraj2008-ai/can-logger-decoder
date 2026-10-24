#define _GNU_SOURCE
#include "can_core.h"
#include "error_codes.h"

#include <string.h>

/* ----------------------------------------------------------------------
 * Bus-off recovery
 *
 * ISO 11898-1: a node in bus-off may rejoin the bus only after it has
 * observed 128 sequences of 11 consecutive recessive bits. This module
 * drives that sequence in software, polling with a caller-supplied
 * millisecond time source so the behavior is deterministic under test.
 * ---------------------------------------------------------------------- */

int bus_off_recovery_init(bus_off_recovery_t *rec) {
    if (rec == NULL) return CAN_ERR_INVALID_ARG;

    memset(rec, 0, sizeof(*rec));
    rec->state = BOR_IDLE;
    rec->max_attempts = BUS_OFF_RECOVERY_MAX_ATTEMPTS;
    rec->timeout_ms = BUS_OFF_RECOVERY_TIMEOUT_MS;
    rec->auto_recover = true;
    return CAN_OK;
}

int bus_off_recovery_start(bus_off_recovery_t *rec) {
    if (rec == NULL) return CAN_ERR_INVALID_ARG;

    rec->state = BOR_WAITING_BUS_IDLE;
    rec->attempts = 0;
    rec->sequences_remaining = BUS_OFF_RECOVERY_SEQUENCES;
    rec->started_ms = 0;
    return CAN_OK;
}

int bus_off_recovery_poll(bus_off_recovery_t *rec, uint64_t now_ms) {
    if (rec == NULL) return CAN_ERR_INVALID_ARG;

    switch (rec->state) {
        case BOR_IDLE:
            return 0; /* not active */

        case BOR_WAITING_BUS_IDLE: {
            if (rec->started_ms == 0) {
                rec->started_ms = now_ms;
            }
            if (rec->wait_for_bus_idle != NULL) {
                int rc = rec->wait_for_bus_idle(rec->ctx,
                                                (int)rec->timeout_ms);
                if (rc < 0) {
                    rec->state = BOR_FAILED;
                    return -1;
                }
                if (rc == 0) {
                    /* Still waiting; check overall timeout */
                    if (now_ms - rec->started_ms >= rec->timeout_ms) {
                        rec->state = BOR_FAILED;
                        return -1;
                    }
                    return 0;
                }
            }
            /* Bus is idle: issue the reset */
            rec->state = BOR_RESET_ISSUED;
            return 0;
        }

        case BOR_RESET_ISSUED: {
            rec->attempts++;
            if (rec->reset_controller != NULL) {
                int rc = rec->reset_controller(rec->ctx);
                if (rc < 0) {
                    if (rec->attempts >= rec->max_attempts) {
                        rec->state = BOR_FAILED;
                        return -1;
                    }
                    /* Retry: go back and wait for the bus to idle again */
                    rec->state = BOR_WAITING_BUS_IDLE;
                    rec->started_ms = now_ms;
                    return 0;
                }
            }
            rec->sequences_remaining = BUS_OFF_RECOVERY_SEQUENCES;
            rec->started_ms = now_ms;
            rec->state = BOR_RECOVERING;
            return 0;
        }

        case BOR_RECOVERING: {
            /* Model the passage of time as recovery-sequence progress.
             * At 1 Mbit/s, 11 recessive bits take 11 us; 128 sequences
             * complete in only a few milliseconds. We conservatively
             * credit one sequence per millisecond of observed time. */
            if (rec->started_ms == 0) {
                rec->started_ms = now_ms;
            }
            uint64_t elapsed = now_ms - rec->started_ms;
            if (elapsed >= rec->sequences_remaining) {
                rec->sequences_remaining = 0;
                rec->state = BOR_RECOVERED;
                return 1;
            }
            rec->sequences_remaining -= (uint32_t)elapsed;
            rec->started_ms = now_ms;

            if (now_ms - rec->started_ms > rec->timeout_ms * 10ULL) {
                rec->state = BOR_FAILED;
                return -1;
            }
            return 0;
        }

        case BOR_RECOVERED:
            return 1;

        case BOR_FAILED:
            return -1;

        default:
            return CAN_ERR_INVALID_ARG;
    }
}

bor_state_t bus_off_recovery_state(const bus_off_recovery_t *rec) {
    return rec ? rec->state : BOR_IDLE;
}

bool bus_off_recovery_is_done(const bus_off_recovery_t *rec) {
    return rec ? rec->state == BOR_RECOVERED : false;
}

bool bus_off_recovery_has_failed(const bus_off_recovery_t *rec) {
    return rec ? rec->state == BOR_FAILED : false;
}

int bus_off_recovery_reset(bus_off_recovery_t *rec) {
    if (rec == NULL) return CAN_ERR_INVALID_ARG;
    rec->state = BOR_IDLE;
    rec->attempts = 0;
    rec->sequences_remaining = 0;
    rec->started_ms = 0;
    return CAN_OK;
}

const char *bor_state_to_string(bor_state_t state) {
    switch (state) {
        case BOR_IDLE:               return "IDLE";
        case BOR_WAITING_BUS_IDLE:   return "WAITING_BUS_IDLE";
        case BOR_RESET_ISSUED:       return "RESET_ISSUED";
        case BOR_RECOVERING:         return "RECOVERING";
        case BOR_RECOVERED:          return "RECOVERED";
        case BOR_FAILED:             return "FAILED";
        default:                     return "UNKNOWN";
    }
}
