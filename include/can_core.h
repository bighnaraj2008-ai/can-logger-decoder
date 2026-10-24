#ifndef CAN_CORE_H
#define CAN_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Library version (semantic, major.minor.patch).
 */
#define CAN_CORE_VERSION_MAJOR 2u
#define CAN_CORE_VERSION_MINOR 0u
#define CAN_CORE_VERSION_PATCH 0u

/* ----------------------------------------------------------------------
 * CAN identifier masks (Linux SocketCAN compatible)
 * ---------------------------------------------------------------------- */
#define CAN_SFF_MASK   0x000007FFU   /**< Mask for 11-bit standard identifiers */
#define CAN_EFF_MASK   0x1FFFFFFFU   /**< Mask for 29-bit extended identifiers */
#define CAN_EFF_FLAG   0x80000000U   /**< Flag bit: extended frame format */
#define CAN_RTR_FLAG   0x40000000U   /**< Flag bit: remote transmission request */
#define CAN_ERR_FLAG   0x20000000U   /**< Flag bit: error frame */

/* ----------------------------------------------------------------------
 * Data Length Code (DLC) limits
 * ---------------------------------------------------------------------- */
#define CAN_DLC_MIN               0u   /**< Minimum valid DLC (0 data bytes) */
#define CAN_DLC_MAX               8u   /**< Maximum valid DLC for classical CAN */
#define CAN_FRAME_MAX_DATA_LEN    8u   /**< Physical payload buffer size in bytes */
#define CAN_TIMESTAMP_ISO_LEN    64u   /**< Size of ISO-8601 timestamp buffers */

/* ----------------------------------------------------------------------
 * Core CAN Frame and Message Data Structures
 * ---------------------------------------------------------------------- */
#ifndef CAN_FRAME_T_DEFINED
#define CAN_FRAME_T_DEFINED
/**
 * @brief On-the-wire / controller-native CAN frame representation.
 */
typedef struct {
    uint32_t can_id;       /**< Raw CAN identifier (may include flag bits) */
    uint8_t  can_dlc;      /**< Raw data length code */
    uint8_t  data[CAN_FRAME_MAX_DATA_LEN];
    bool     is_extended;  /**< Derived: true when 29-bit extended format */
    bool     is_rtr;       /**< Derived: true when remote transmission request */
    bool     is_error;     /**< Derived: true when error frame */
    uint8_t  checksum;     /**< Trailing payload checksum byte (when used) */
    bool     checksum_valid; /**< Set when the trailing checksum was verified */
} can_frame_t;
#endif

#ifndef CAN_MSG_T_DEFINED
#define CAN_MSG_T_DEFINED
/**
 * @brief Host-order CAN message with metadata (timestamps, flags).
 */
typedef struct {
    uint32_t can_id;          /**< Clean CAN identifier (flags stripped) */
    uint8_t  dlc;             /**< Validated data length (0-8 bytes) */
    uint8_t  data[CAN_FRAME_MAX_DATA_LEN];
    double   timestamp_sec;   /**< Monotonic time in seconds */
    char     timestamp_iso[CAN_TIMESTAMP_ISO_LEN]; /**< ISO-8601 timestamp */
    bool     is_extended;     /**< True if 29-bit extended frame */
    bool     is_rtr;          /**< True if Remote Transmission Request */
    bool     is_error;        /**< True if error frame */
    uint8_t  checksum;        /**< Payload checksum byte (when present) */
    bool     checksum_valid;  /**< True when payload checksum verified */
} can_msg_t;
#endif

/* ----------------------------------------------------------------------
 * Fault Confinement State Machine Types
 * ---------------------------------------------------------------------- */
#define ERR_CONF_WARNING_THRESHOLD  96u
#define ERR_CONF_PASSIVE_THRESHOLD 127u
#define ERR_CONF_BUS_OFF_LIMIT     256u

typedef enum {
    ERR_STATE_NORMAL = 0,      /**< Bus healthy, counters within limits */
    ERR_STATE_WARNING,         /**< TEC or REC >= 96 */
    ERR_STATE_ERROR_PASSIVE,   /**< TEC or REC >= 127 */
    ERR_STATE_BUS_OFF,         /**< TEC > 255, node off the bus */
    ERR_STATE_RECOVERING,      /**< Bus-off recovery sequence in progress */
    ERR_STATE_RESET            /**< Controller reset requested / in progress */
} err_state_t;

typedef enum {
    EVT_NONE = 0,
    EVT_RX_OK,                 /**< Frame received without error */
    EVT_TX_OK,                 /**< Frame transmitted without error */
    EVT_ERROR_FRAME,           /**< Error frame detected on the bus */
    EVT_SPI_FAULT,             /**< MCP2515 SPI communication fault */
    EVT_SOCKET_OVERFLOW,       /**< SocketCAN receive buffer overflow */
    EVT_RX_BUFFER_OVERFLOW,    /**< Internal frame queue overflow */
    EVT_BUS_OFF_DETECTED,      /**< Controller reported bus-off */
    EVT_RECOVERY_TIMEOUT,      /**< Recovery sequence timed out */
    EVT_RESET_COMPLETE,        /**< Controller reset finished successfully */
    EVT_MANUAL_RESET,          /**< Operator-initiated reset */
    EVT_MAX
} err_event_t;

typedef struct {
    err_state_t  state;               /**< Current state */
    err_state_t  previous_state;      /**< State before the last transition */
    uint32_t     tx_error_count;      /**< Transmit error counter (TEC-like) */
    uint32_t     rx_error_count;      /**< Receive error counter (REC-like) */
    uint32_t     fault_count;         /**< Total fault events seen */
    uint32_t     recovery_count;      /**< Successful recoveries performed */
    uint64_t     last_event_time_ms;  /**< Timestamp of the last event */
    bool         auto_recovery;       /**< Attempt recovery automatically */
    void        *ctx;                 /**< Opaque application context */
    int         (*on_enter_bus_off)(void *ctx);
    int         (*on_enter_reset)(void *ctx);
    int         (*on_recovered)(void *ctx);
} error_machine_t;

/* ----------------------------------------------------------------------
 * Bus-Off Recovery Controller Types
 * ---------------------------------------------------------------------- */
#define BUS_OFF_RECOVERY_SEQUENCES 128u
#define BUS_OFF_RECOVERY_MAX_ATTEMPTS 8u
#define BUS_OFF_RECOVERY_TIMEOUT_MS 5000u

typedef enum {
    BOR_IDLE = 0,              /**< Not active */
    BOR_WAITING_BUS_IDLE,      /**< Waiting for the bus to become idle */
    BOR_RESET_ISSUED,          /**< Reset command sent to the controller */
    BOR_RECOVERING,            /**< Counting the 128 recovery sequences */
    BOR_RECOVERED,             /**< Successfully rejoined the bus */
    BOR_FAILED                 /**< Recovery failed */
} bor_state_t;

typedef struct {
    bor_state_t  state;
    uint32_t     attempts;
    uint32_t     max_attempts;
    uint32_t     sequences_remaining;
    uint64_t     started_ms;
    uint64_t     timeout_ms;
    bool         auto_recover;
    void        *ctx;
    int         (*reset_controller)(void *ctx);
    int         (*wait_for_bus_idle)(void *ctx, int timeout_ms);
} bus_off_recovery_t;

/* ----------------------------------------------------------------------
 * Function Declarations
 * ---------------------------------------------------------------------- */
/* DLC and Identifier utilities */
int can_dlc_to_bytes(uint8_t dlc);
int can_bytes_to_dlc(size_t length);
bool can_dlc_is_valid(uint8_t dlc);
bool can_id_is_extended(uint32_t raw_id);
uint32_t can_id_clean(uint32_t raw_id);
uint8_t can_dlc_to_length(uint8_t dlc);
const char *can_core_version(void);

/* Frame and wire conversions */
void can_frame_init(can_frame_t *frame);
void can_msg_clear(can_msg_t *msg);
bool can_frame_is_valid(const can_frame_t *frame);
can_error_t can_frame_to_wire(const can_frame_t *frame, uint8_t *wire, size_t *wire_len);
can_error_t can_frame_from_wire(const uint8_t *wire, size_t wire_len, can_frame_t *frame);
void can_frame_copy(can_frame_t *dst, const can_frame_t *src);

/* Checksum utilities */
uint8_t can_checksum_compute(const uint8_t *data, size_t len);
void can_checksum_append(uint8_t *data, size_t len);
bool can_checksum_validate(const uint8_t *data, size_t len, uint8_t checksum);

/* Error state machine functions */
int error_machine_init(error_machine_t *m);
int error_machine_process_event(error_machine_t *m, err_event_t evt);
int error_machine_inject_error(error_machine_t *m, uint32_t tx_inc, uint32_t rx_inc);
err_state_t error_machine_current_state(const error_machine_t *m);
err_state_t error_machine_get_state(const error_machine_t *m);
bool error_machine_is_bus_off(const error_machine_t *m);
bool error_machine_is_healthy(const error_machine_t *m);
const char *error_machine_state_name(err_state_t state);
const char *err_state_to_string(err_state_t state);
const char *error_machine_event_name(err_event_t evt);
int error_machine_reset(error_machine_t *m);

/* Bus-off recovery controller functions */
int bus_off_recovery_init(bus_off_recovery_t *rec);
int bus_off_recovery_start(bus_off_recovery_t *rec);
int bus_off_recovery_poll(bus_off_recovery_t *rec, uint64_t now_ms);
bor_state_t bus_off_recovery_state(const bus_off_recovery_t *rec);
bool bus_off_recovery_is_done(const bus_off_recovery_t *rec);
bool bus_off_recovery_has_failed(const bus_off_recovery_t *rec);
int bus_off_recovery_reset(bus_off_recovery_t *rec);
const char *bor_state_to_string(bor_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* CAN_CORE_H */
