#ifndef VCAN_INTERFACE_H
#define VCAN_INTERFACE_H

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "can_core.h"
#include "can_filters.h"
#include "error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Virtual CAN (SocketCAN) socket handle.
 *
 * Encapsulates the socket descriptor, interface name, binding state,
 * and error details for cross-platform virtual bus communication.
 */
typedef struct {
    int    fd;          /**< Socket file descriptor (-1 when closed) */
    int    ifindex;     /**< Kernel interface index */
    char   ifname[64];  /**< Interface name, e.g. "vcan0" */
    bool   bound;       /**< Whether bind() succeeded */
    bool   nonblock;    /**< Non-blocking I/O mode */
    int    last_err;    /**< can_error_t from the last failing operation */
} vcan_socket_t;

#if defined(__linux__)
    #include <sys/socket.h>
    #include <linux/can.h>
    #include <linux/can/raw.h>
    #define VCAN_HAS_SOCKETCAN 1
#else
    #define VCAN_HAS_SOCKETCAN 0
#endif

/* ----------------------------------------------------------------------
 * Socket lifecycle and configuration
 * ---------------------------------------------------------------------- */

/**
 * @brief Create an AF_CAN / CAN_RAW socket.
 *
 * @return Socket file descriptor on success, or negative can_error_t.
 */
int vcan_socket_open(void);

/**
 * @brief Bind the socket to a virtual CAN interface (e.g. "vcan0").
 *
 * @param sock Socket handle to bind
 * @param ifname Network interface name
 * @return CAN_OK on success, or negative can_error_t.
 */
int vcan_socket_bind(vcan_socket_t *sock, const char *ifname);

/**
 * @brief Enable or disable non-blocking I/O on the socket descriptor.
 */
int vcan_socket_set_nonblock(vcan_socket_t *sock, bool nonblock);

/**
 * @brief Close the socket and reset handle fields.
 */
int vcan_socket_close(vcan_socket_t *sock);

/**
 * @brief Return a human-readable description for a socket error code.
 */
const char *vcan_socket_strerror(int err);

/* ----------------------------------------------------------------------
 * Frame I/O
 * ---------------------------------------------------------------------- */

/**
 * @brief Read a single CAN frame from the virtual socket.
 *
 * @param sock Active virtual socket handle
 * @param frame Output frame structure
 * @param timeout_ms Timeout in milliseconds (-1 blocking, 0 non-blocking)
 * @return 1 on frame received, 0 on timeout, or negative can_error_t on error.
 */
int vcan_socket_read_frame(vcan_socket_t *sock, can_frame_t *frame, int timeout_ms);

/**
 * @brief Transmit a single CAN frame to the virtual socket.
 *
 * @param sock Active virtual socket handle
 * @param frame Frame to write
 * @return 1 on frame transmitted, or negative can_error_t on error.
 */
int vcan_socket_write_frame(vcan_socket_t *sock, const can_frame_t *frame);

/* ----------------------------------------------------------------------
 * Frame routing and polling
 * ---------------------------------------------------------------------- */

/**
 * @brief Frame handler callback used by vcan_poll_loop().
 *
 * @return 0 to continue polling, non-zero to exit the loop.
 */
typedef int (*vcan_frame_handler_fn)(const can_frame_t *frame, void *user_data);

/**
 * @brief High-efficiency polling loop: receives frames and dispatches to handler.
 */
int vcan_poll_loop(vcan_socket_t *sock, vcan_frame_handler_fn handler,
                   void *user_data, int timeout_ms,
                   volatile sig_atomic_t *running);

/**
 * @brief Route a frame through an acceptance filter set.
 */
int vcan_route_frame(const can_frame_t *frame, const can_filter_set_t *filters);

#ifdef __cplusplus
}
#endif

#endif /* VCAN_INTERFACE_H */
