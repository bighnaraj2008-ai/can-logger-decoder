#define _GNU_SOURCE
#include "../include/can_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#if CAN_HAS_SOCKETCAN
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <net/if.h>
    #include <linux/can.h>
    #include <linux/can/raw.h>
#endif

struct can_bus {
    char interface[64];
    int socket_fd;
    bool is_mock;
    int mock_rate_hz;
    uint64_t frame_sequence;
    struct timespec start_tp;
};

can_bus_t* can_bus_init(const char *interface_name, bool force_mock, int mock_rate_hz) {
    can_bus_t *bus = (can_bus_t*)calloc(1, sizeof(can_bus_t));
    if (!bus) return NULL;

    if (interface_name && strlen(interface_name) > 0) {
        snprintf(bus->interface, sizeof(bus->interface), "%s", interface_name);
    } else {
        snprintf(bus->interface, sizeof(bus->interface), "vcan0");
    }

    bus->socket_fd = -1;
    bus->mock_rate_hz = (mock_rate_hz > 0) ? mock_rate_hz : 10;
    bus->frame_sequence = 0;
    clock_gettime(CLOCK_MONOTONIC, &bus->start_tp);

#if CAN_HAS_SOCKETCAN
    if (!force_mock) {
        int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (s >= 0) {
            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%.15s", bus->interface);

            if (ioctl(s, SIOCGIFINDEX, &ifr) >= 0) {
                struct sockaddr_can addr;
                memset(&addr, 0, sizeof(addr));
                addr.can_family = AF_CAN;
                addr.can_ifindex = ifr.ifr_ifindex;

                if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) >= 0) {
                    bus->socket_fd = s;
                    bus->is_mock = false;
                    return bus;
                }
            }
            close(s);
        }
    }
#endif

    bus->is_mock = true;
    bus->socket_fd = -1;
    return bus;
}

static double get_monotonic_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
}

static void get_current_iso_time(char *buf, size_t len) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm_info;
    gmtime_r(&ts.tv_sec, &tm_info);
    unsigned int ms = (unsigned int)(ts.tv_nsec / 1000000L);
    snprintf(buf, len, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
             (unsigned int)(tm_info.tm_year + 1900),
             (unsigned int)(tm_info.tm_mon + 1),
             (unsigned int)tm_info.tm_mday,
             (unsigned int)tm_info.tm_hour,
             (unsigned int)tm_info.tm_min,
             (unsigned int)tm_info.tm_sec,
             ms);
}

int can_bus_read(can_bus_t *bus, can_msg_t *msg, int timeout_ms) {
    if (!bus || !msg) return -1;

#if CAN_HAS_SOCKETCAN
    if (!bus->is_mock) {
        struct timeval tv;
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(bus->socket_fd, &read_fds);

        if (timeout_ms >= 0) {
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
        }

        int ret = select(bus->socket_fd + 1, &read_fds, NULL, NULL, (timeout_ms >= 0) ? &tv : NULL);
        if (ret < 0) {
            if (errno == EINTR) return 0;
            return -1;
        } else if (ret == 0) {
            return 0;
        }

        struct can_frame frame;
        ssize_t nbytes = read(bus->socket_fd, &frame, sizeof(struct can_frame));
        if (nbytes < (ssize_t)sizeof(struct can_frame)) return -1;

        msg->is_extended = ((frame.can_id & CAN_EFF_FLAG) != 0);
        msg->is_rtr = ((frame.can_id & CAN_RTR_FLAG) != 0);
        msg->is_error = ((frame.can_id & CAN_ERR_FLAG) != 0);

        msg->can_id = frame.can_id & (msg->is_extended ? CAN_EFF_MASK : CAN_SFF_MASK);
        msg->dlc = (frame.can_dlc > CAN_MAX_DATA_LEN) ? CAN_MAX_DATA_LEN : frame.can_dlc;

        memset(msg->data, 0, sizeof(msg->data));
        memcpy(msg->data, frame.data, msg->dlc);

        msg->timestamp_sec = get_monotonic_sec();
        get_current_iso_time(msg->timestamp_iso, sizeof(msg->timestamp_iso));

        return 1;
    }
#endif
    return 0;
}
