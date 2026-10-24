#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Canonical error codes used across the CAN logger/decoder stack.
 *
 * All error codes are negative; CAN_OK (0) denotes success.
 */
typedef enum {
    CAN_OK = 0,                    /**< No error / operation successful */
    CAN_ERR_NOMEM = -1,            /**< Memory allocation failure */
    CAN_ERR_IO = -2,               /**< Generic I/O / bus communication failure */
    CAN_ERR_TIMEOUT = -3,          /**< Operation timed out */
    CAN_ERR_INVALID_ARG = -4,      /**< Invalid argument passed to function */
    CAN_ERR_INVALID_DLC = -5,      /**< Data Length Code out of bounds [0..8] */
    CAN_ERR_CHECKSUM = -6,         /**< Payload checksum validation failure */
    CAN_ERR_BUFFER_OVERFLOW = -7,  /**< Socket/ring buffer overflow */
    CAN_ERR_SPI_FAULT = -8,        /**< MCP2515 SPI communication fault */
    CAN_ERR_BUS_OFF = -9,          /**< CAN controller entered bus-off state */
    CAN_ERR_NOT_INIT = -10,        /**< Handle used before initialization */
    CAN_ERR_NO_MESSAGE = -11,      /**< No message available (non-blocking empty) */
    CAN_ERR_UNSUPPORTED = -12,     /**< Requested feature not supported on platform */
    CAN_ERR_INTERNAL = -13         /**< Internal consistency failure */
} can_error_t;

/**
 * @brief Return a human-readable string for a can_error_t value.
 *
 * @param err Error code to describe
 * @return const char* Static NUL-terminated description (never NULL)
 */
static inline const char *can_error_string(can_error_t err) {
    switch (err) {
        case CAN_OK:                  return "Success (no error)";
        case CAN_ERR_NOMEM:            return "Out of memory";
        case CAN_ERR_IO:               return "I/O or communication error";
        case CAN_ERR_TIMEOUT:          return "Operation timed out";
        case CAN_ERR_INVALID_ARG:      return "Invalid argument";
        case CAN_ERR_INVALID_DLC:      return "Invalid Data Length Code";
        case CAN_ERR_CHECKSUM:         return "Payload checksum verification failed";
        case CAN_ERR_BUFFER_OVERFLOW:  return "Buffer or socket queue overflow";
        case CAN_ERR_SPI_FAULT:        return "MCP2515 SPI communication fault";
        case CAN_ERR_BUS_OFF:          return "Controller entered bus-off state";
        case CAN_ERR_NOT_INIT:         return "Subsystem or handle not initialized";
        case CAN_ERR_NO_MESSAGE:       return "No message available";
        case CAN_ERR_UNSUPPORTED:      return "Feature unsupported on current platform";
        case CAN_ERR_INTERNAL:         return "Internal assertion or logic failure";
        default:                       return "Unknown CAN error";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* ERROR_CODES_H */
