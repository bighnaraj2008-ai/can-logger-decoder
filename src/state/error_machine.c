#define _GNU_SOURCE
#include "can_core.h"
#include "error_codes.h"

#include <string.h>
#include <time.h>

/* ----------------------------------------------------------------------
 * Fault-confinement thresholds (mirroring the CAN specification):
 *   TEC/REC >= 96   -> error warning
 *   TEC/REC >= 127  -> error passive
 *   TEC > 255       -> bus-off
 * ---------------------------------------------------------------------- */

static uint64_t time_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void decay_counters(error_machine_t *m) {
    if (m->tx_error_count > 0u) m->tx_error_count--;
    if (m->rx_error_count > 0u) m->rx_error_count--;
}

static void promote_by_counters(error_machine_t *m) {
    if (m->tx_error_count >= ERR_CONF_BUS_OFF_LIMIT ||
        m->rx_error_count >= ERR_CONF_BUS_OFF_LIMIT) {
        /* Bus-off is signalled through the bus-off detected event path */
        return;
    }
    if (m->tx_error_count >= ERR_CONF_PASSIVE_THRESHOLD ||
        m->rx_error_count >= ERR_CONF_PASSIVE_THRESHOLD) {
        if (m->state != ERR_STATE_ERROR_PASSIVE) {
            m->previous_state = m->state;
            m->state = ERR_STATE_ERROR_PASSIVE;
        }
        return;
    }
    if (m->tx_error_count >= ERR_CONF_WARNING_THRESHOLD ||
        m->rx_error_count >= ERR_CONF_WARNING_THRESHOLD) {
        if (m->state == ERR_STATE_NORMAL) {
            m->previous_state = m->state;
            m->state = ERR_STATE_WARNING;
        }
        return;
    }
    if (m->state == ERR_STATE_WARNING ||
        m->state == ERR_STATE_ERROR_PASSIVE) {
        m->previous_state = m->state;
        m->state = ERR_STATE_NORMAL;
    }
}

static int enter_state(error_machine_t *m, err_state_t next) {
    if (m->state == next) return CAN_OK;
    m->previous_state = m->state;
    m->state = next;
    m->last_event_time_ms = time_now_ms();

    if (next == ERR_STATE_BUS_OFF && m->on_enter_bus_off != NULL) {
        return m->on_enter_bus_off(m->ctx);
    }
    if (next == ERR_STATE_RESET && m->on_enter_reset != NULL) {
        return m->on_enter_reset(m->ctx);
    }
    return CAN_OK;
}

int error_machine_init(error_machine_t *m) {
    if (m == NULL) return CAN_ERR_INVALID_ARG;

    memset(m, 0, sizeof(*m));
    m->state = ERR_STATE_NORMAL;
    m->previous_state = ERR_STATE_NORMAL;
    m->auto_recovery = true;
    m->last_event_time_ms = time_now_ms();
    return CAN_OK;
}

int error_machine_reset(error_machine_t *m) {
    if (m == NULL) return CAN_ERR_INVALID_ARG;
    void *ctx = m->ctx;
    int (*on_recovered)(void *) = m->on_recovered;
    memset(m, 0, sizeof(*m));
    m->state = ERR_STATE_NORMAL;
    m->previous_state = ERR_STATE_NORMAL;
    m->auto_recovery = true;
    m->ctx = ctx;
    m->on_recovered = on_recovered;
    m->last_event_time_ms = time_now_ms();
    return CAN_OK;
}

int error_machine_process_event(error_machine_t *m, err_event_t event) {
    if (m == NULL) return CAN_ERR_INVALID_ARG;
    if (event <= EVT_NONE || event >= EVT_MAX) return CAN_ERR_INVALID_ARG;

    m->last_event_time_ms = time_now_ms();

    switch (m->state) {
        case ERR_STATE_NORMAL:
            switch (event) {
                case EVT_RX_OK:
                case EVT_TX_OK:
                    decay_counters(m);
                    break;
                case EVT_ERROR_FRAME: {
                    m->tx_error_count++;
                    m->rx_error_count++;
                    m->fault_count++;
                    promote_by_counters(m);
                    break;
                }
                case EVT_SPI_FAULT:
                    m->fault_count++;
                    return enter_state(m, ERR_STATE_RESET);
                case EVT_SOCKET_OVERFLOW:
                case EVT_RX_BUFFER_OVERFLOW:
                    m->fault_count++;
                    if (m->state == ERR_STATE_NORMAL) {
                        m->previous_state = m->state;
                        m->state = ERR_STATE_WARNING;
                    }
                    break;
                case EVT_BUS_OFF_DETECTED:
                    return enter_state(m, ERR_STATE_BUS_OFF);
                case EVT_MANUAL_RESET:
                    return enter_state(m, ERR_STATE_RESET);
                default:
                    break;
            }
            break;

        case ERR_STATE_WARNING:
            switch (event) {
                case EVT_RX_OK:
                case EVT_TX_OK:
                    decay_counters(m);
                    if (m->tx_error_count < ERR_CONF_WARNING_THRESHOLD &&
                        m->rx_error_count < ERR_CONF_WARNING_THRESHOLD) {
                        m->previous_state = m->state;
                        m->state = ERR_STATE_NORMAL;
                    }
                    break;
                case EVT_ERROR_FRAME: {
                    m->tx_error_count++;
                    m->rx_error_count++;
                    m->fault_count++;
                    if (m->tx_error_count >= ERR_CONF_PASSIVE_THRESHOLD ||
                        m->rx_error_count >= ERR_CONF_PASSIVE_THRESHOLD) {
                        return enter_state(m, ERR_STATE_ERROR_PASSIVE);
                    }
                    break;
                }
                case EVT_SPI_FAULT:
                    m->fault_count++;
                    return enter_state(m, ERR_STATE_RESET);
                case EVT_SOCKET_OVERFLOW:
                case EVT_RX_BUFFER_OVERFLOW:
                    m->fault_count++;
                    break;
                case EVT_BUS_OFF_DETECTED:
                    return enter_state(m, ERR_STATE_BUS_OFF);
                case EVT_MANUAL_RESET:
                    return enter_state(m, ERR_STATE_RESET);
                default:
                    break;
            }
            break;

        case ERR_STATE_ERROR_PASSIVE:
            switch (event) {
                case EVT_RX_OK:
                case EVT_TX_OK:
                    decay_counters(m);
                    if (m->tx_error_count < ERR_CONF_PASSIVE_THRESHOLD &&
                        m->rx_error_count < ERR_CONF_PASSIVE_THRESHOLD &&
                        (m->tx_error_count < ERR_CONF_WARNING_THRESHOLD &&
                         m->rx_error_count < ERR_CONF_WARNING_THRESHOLD)) {
                        m->previous_state = m->state;
                        m->state = ERR_STATE_NORMAL;
                    } else if (m->tx_error_count < ERR_CONF_PASSIVE_THRESHOLD &&
                               m->rx_error_count < ERR_CONF_PASSIVE_THRESHOLD) {
                        m->previous_state = m->state;
                        m->state = ERR_STATE_WARNING;
                    }
                    break;
                case EVT_ERROR_FRAME:
                    m->tx_error_count++;
                    m->rx_error_count++;
                    m->fault_count++;
                    break;
                case EVT_SPI_FAULT:
                    m->fault_count++;
                    return enter_state(m, ERR_STATE_RESET);
                case EVT_BUS_OFF_DETECTED:
                    return enter_state(m, ERR_STATE_BUS_OFF);
                case EVT_MANUAL_RESET:
                    return enter_state(m, ERR_STATE_RESET);
                default:
                    break;
            }
            break;

        case ERR_STATE_BUS_OFF:
            switch (event) {
                case EVT_RECOVERY_TIMEOUT:
                    if (m->auto_recovery) {
                        return enter_state(m, ERR_STATE_RECOVERING);
                    }
                    break;
                case EVT_RESET_COMPLETE:
                    m->previous_state = m->state;
                    m->state = ERR_STATE_NORMAL;
                    m->tx_error_count = 0;
                    m->rx_error_count = 0;
                    m->recovery_count++;
                    if (m->on_recovered != NULL) {
                        return m->on_recovered(m->ctx);
                    }
                    break;
                case EVT_MANUAL_RESET:
                    return enter_state(m, ERR_STATE_RESET);
                case EVT_SPI_FAULT:
                    m->fault_count++;
                    break;
                default:
                    break;
            }
            break;

        case ERR_STATE_RECOVERING:
            switch (event) {
                case EVT_RESET_COMPLETE:
                    m->previous_state = m->state;
                    m->state = ERR_STATE_NORMAL;
                    m->tx_error_count = 0;
                    m->rx_error_count = 0;
                    m->recovery_count++;
                    if (m->on_recovered != NULL) {
                        return m->on_recovered(m->ctx);
                    }
                    break;
                case EVT_RECOVERY_TIMEOUT:
                    return enter_state(m, ERR_STATE_BUS_OFF);
                case EVT_MANUAL_RESET:
                    return enter_state(m, ERR_STATE_RESET);
                default:
                    break;
            }
            break;

        case ERR_STATE_RESET:
            switch (event) {
                case EVT_RESET_COMPLETE:
                    m->previous_state = m->state;
                    m->state = ERR_STATE_NORMAL;
                    m->tx_error_count = 0;
                    m->rx_error_count = 0;
                    m->recovery_count++;
                    if (m->on_recovered != NULL) {
                        return m->on_recovered(m->ctx);
                    }
                    break;
                case EVT_RECOVERY_TIMEOUT:
                    return enter_state(m, ERR_STATE_RECOVERING);
                default:
                    break;
            }
            break;
    }

    m->last_event_time_ms = time_now_ms();
    return CAN_OK;
}

int error_machine_inject_error(error_machine_t *m, uint32_t tx_inc,
                               uint32_t rx_inc) {
    if (m == NULL) return CAN_ERR_INVALID_ARG;

    m->tx_error_count += tx_inc;
    m->rx_error_count += rx_inc;
    m->fault_count++;
    m->last_event_time_ms = time_now_ms();

    if (m->tx_error_count >= ERR_CONF_BUS_OFF_LIMIT ||
        m->rx_error_count >= ERR_CONF_BUS_OFF_LIMIT) {
        if (m->state != ERR_STATE_BUS_OFF) {
            return enter_state(m, ERR_STATE_BUS_OFF);
        }
        return CAN_OK;
    }
    promote_by_counters(m);
    return CAN_OK;
}

err_state_t error_machine_get_state(const error_machine_t *m) {
    return m ? m->state : ERR_STATE_NORMAL;
}

bool error_machine_is_bus_off(const error_machine_t *m) {
    return m ? m->state == ERR_STATE_BUS_OFF : false;
}

bool error_machine_is_healthy(const error_machine_t *m) {
    return m ? (m->state == ERR_STATE_NORMAL || m->state == ERR_STATE_WARNING)
             : false;
}

const char *err_state_to_string(err_state_t state) {
    switch (state) {
        case ERR_STATE_NORMAL:        return "NORMAL";
        case ERR_STATE_WARNING:       return "WARNING";
        case ERR_STATE_ERROR_PASSIVE: return "ERROR_PASSIVE";
        case ERR_STATE_BUS_OFF:       return "BUS_OFF";
        case ERR_STATE_RECOVERING:    return "RECOVERING";
        case ERR_STATE_RESET:         return "RESET";
        default:                      return "UNKNOWN";
    }
}
