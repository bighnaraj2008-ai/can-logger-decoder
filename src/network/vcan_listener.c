#define _GNU_SOURCE
#include "vcan_interface.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if VCAN_HAS_SOCKETCAN
    #include <sys/select.h>
    #include <unistd.h>

int vcan_socket_read_frame(vcan_socket_t *sock, can_frame_t *frame, int timeout_ms) {
    if (sock == NULL || frame == NULL) return CAN_ERR_INVALID_ARG;
    if (!sock->bound || sock->fd < 0) return CAN_ERR_NOT_INIT;

    can_frame_init(frame);

    if (timeout_ms > 0) {
        struct timeval tv;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock->fd, &fds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (long)(timeout_ms % 1000) * 1000L;

        int ret = select(sock->fd + 1, &fds, NULL, NULL, &tv);
        if (ret < 0) {
            sock->last_err = CAN_ERR_IO;
            return CAN_ERR_IO;
        }
        if (ret == 0) return 0; /* timeout, no frame */
    }

    struct can_frame wire;
    ssize_t nbytes = read(sock->fd, &wire, sizeof(wire));
    if (nbytes < (ssize_t)sizeof(wire)) {
        sock->last_err = CAN_ERR_IO;
        return CAN_ERR_IO;
    }

    frame->can_id = wire.can_id & (wire.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK);
    frame->can_dlc = wire.can_dlc > CAN_FRAME_MAX_DATA_LEN ? CAN_FRAME_MAX_DATA_LEN : wire.can_dlc;
    memcpy(frame->data, wire.data, (size_t)frame->can_dlc);
    frame->is_extended = ((wire.can_id & CAN_EFF_FLAG) != 0u);
    frame->is_rtr = ((wire.can_id & CAN_RTR_FLAG) != 0u);
    frame->is_error = ((wire.can_id & CAN_ERR_FLAG) != 0u);

    sock->last_err = (int)CAN_OK;
    return 1;
}

int vcan_socket_write_frame(vcan_socket_t *sock, const can_frame_t *frame) {
    if (sock == NULL || frame == NULL) return CAN_ERR_INVALID_ARG;
    if (!sock->bound || sock->fd < 0) return CAN_ERR_NOT_INIT;

    struct can_frame wire;
    memset(&wire, 0, sizeof(wire));
    wire.can_id = frame->can_id | (frame->is_extended ? CAN_EFF_FLAG : 0u) |
                  (frame->is_rtr ? CAN_RTR_FLAG : 0u) |
                  (frame->is_error ? CAN_ERR_FLAG : 0u);
    wire.can_dlc = frame->can_dlc;
    if (wire.can_dlc > CAN_FRAME_MAX_DATA_LEN) wire.can_dlc = CAN_FRAME_MAX_DATA_LEN;
    memcpy(wire.data, frame->data, (size_t)wire.can_dlc);

    ssize_t written = write(sock->fd, &wire, sizeof(wire));
    if (written < (ssize_t)sizeof(wire)) {
        sock->last_err = CAN_ERR_IO;
        return CAN_ERR_IO;
    }
    sock->last_err = (int)CAN_OK;
    return 1;
}

int vcan_poll_loop(vcan_socket_t *sock, vcan_frame_handler_fn handler,
                   void *user_data, int timeout_ms,
                   volatile sig_atomic_t *running) {
    if (sock == NULL || handler == NULL) return CAN_ERR_INVALID_ARG;

    int dispatched = 0;
    while (running == NULL || *running) {
        can_frame_t frame;
        int rc = vcan_socket_read_frame(sock, &frame, timeout_ms);
        if (rc < 0) return rc;
        if (rc == 0) continue;

        dispatched++;
        if (handler(&frame, user_data) != 0) break;
    }
    return dispatched;
}

#else /* !VCAN_HAS_SOCKETCAN */

int vcan_socket_read_frame(vcan_socket_t *sock, can_frame_t *frame, int timeout_ms) {
    (void)timeout_ms;
    if (sock == NULL || frame == NULL) return CAN_ERR_INVALID_ARG;
    if (!sock->bound) return CAN_ERR_NOT_INIT;

    can_frame_init(frame);
    frame->can_id = 0x100;
    frame->can_dlc = 8;
    for (int i = 0; i < 8; i++) frame->data[i] = (uint8_t)(i * 0x11);
    return 1;
}

int vcan_socket_write_frame(vcan_socket_t *sock, const can_frame_t *frame) {
    if (sock == NULL || frame == NULL) return CAN_ERR_INVALID_ARG;
    if (!sock->bound) return CAN_ERR_NOT_INIT;
    return 1;
}

int vcan_poll_loop(vcan_socket_t *sock, vcan_frame_handler_fn handler,
                   void *user_data, int timeout_ms,
                   volatile sig_atomic_t *running) {
    if (sock == NULL || handler == NULL) return CAN_ERR_INVALID_ARG;
    int count = 0;
    can_frame_t frame;
    while ((running == NULL || *running) && count < 10) {
        if (vcan_socket_read_frame(sock, &frame, timeout_ms) == 1) {
            count++;
            if (handler(&frame, user_data) != 0) break;
        }
    }
    return count;
}

#endif /* VCAN_HAS_SOCKETCAN */
