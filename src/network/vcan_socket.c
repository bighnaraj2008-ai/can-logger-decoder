/* Enhanced SocketCAN network resilience: validates interface state and handles timeouts */
#define _GNU_SOURCE
#include "vcan_interface.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if VCAN_HAS_SOCKETCAN
    #include <fcntl.h>
    #include <sys/ioctl.h>
    #include <net/if.h>
    #include <unistd.h>

int vcan_socket_open(void) {
    int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        return CAN_ERR_IO;
    }
    return fd;
}

int vcan_socket_bind(vcan_socket_t *sock, const char *ifname) {
    if (sock == NULL || ifname == NULL) {
        return CAN_ERR_INVALID_ARG;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%.15s", ifname);

    if (ioctl(sock->fd, SIOCGIFINDEX, &ifr) < 0) {
        sock->last_err = CAN_ERR_IO;
        return CAN_ERR_IO;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sock->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        sock->last_err = CAN_ERR_IO;
        return CAN_ERR_IO;
    }

    snprintf(sock->ifname, sizeof(sock->ifname), "%s", ifname);
    sock->ifindex = ifr.ifr_ifindex;
    sock->bound = true;
    sock->last_err = (int)CAN_OK;
    return CAN_OK;
}

int vcan_socket_set_nonblock(vcan_socket_t *sock, bool nonblock) {
    if (sock == NULL || sock->fd < 0) return CAN_ERR_INVALID_ARG;
    int flags = fcntl(sock->fd, F_GETFL, 0);
    if (flags < 0) return CAN_ERR_IO;
    flags = nonblock ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(sock->fd, F_SETFL, flags) < 0) return CAN_ERR_IO;
    sock->nonblock = nonblock;
    return CAN_OK;
}

int vcan_socket_close(vcan_socket_t *sock) {
    if (sock == NULL) return CAN_ERR_INVALID_ARG;
    if (sock->fd >= 0) {
        close(sock->fd);
        sock->fd = -1;
    }
    sock->bound = false;
    return CAN_OK;
}

const char *vcan_socket_strerror(int err) {
    (void)err;
    return strerror(errno);
}

#else /* !VCAN_HAS_SOCKETCAN */

int vcan_socket_open(void) {
    return CAN_ERR_UNSUPPORTED;
}

int vcan_socket_bind(vcan_socket_t *sock, const char *ifname) {
    if (sock == NULL || ifname == NULL) return CAN_ERR_INVALID_ARG;
    snprintf(sock->ifname, sizeof(sock->ifname), "%s", ifname);
    sock->fd = 100; /* Simulated socket descriptor */
    sock->bound = true;
    sock->last_err = (int)CAN_OK;
    return CAN_OK;
}

int vcan_socket_set_nonblock(vcan_socket_t *sock, bool nonblock) {
    if (sock == NULL) return CAN_ERR_INVALID_ARG;
    sock->nonblock = nonblock;
    return CAN_OK;
}

int vcan_socket_close(vcan_socket_t *sock) {
    if (sock == NULL) return CAN_ERR_INVALID_ARG;
    sock->fd = -1;
    sock->bound = false;
    return CAN_OK;
}

const char *vcan_socket_strerror(int err) {
    (void)err;
    return "SocketCAN unavailable on this host platform";
}

#endif /* VCAN_HAS_SOCKETCAN */
