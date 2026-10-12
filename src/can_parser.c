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
