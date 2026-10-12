#define _GNU_SOURCE
#include "../include/can_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#if CAN_HAS_SOCKETCAN
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <sys/time.h>
    #include <sys/select.h>
    #include <net/if.h>
    #include <linux/can.h>
    #include <linux/can/raw.h>
    #include <fcntl.h>
#endif

struct can_bus {
    char interface[64];
    int socket_fd;
    bool is_mock;
    int mock_rate_hz;
    uint64_t frame_sequence;
    struct timespec start_tp;

    /* Internal simulation state for mock generator */
    double sim_time_sec;
    double sim_rpm;
    double sim_speed;
    double sim_throttle;
    double sim_coolant;
    double sim_oil_temp;
    double sim_voltage;
    double sim_brake_press;
    int    sim_gear;
};

/* Helper to get monotonic seconds */
static double get_monotonic_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
}

/* Helper to generate current ISO timestamp */
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

can_bus_t* can_bus_init(const char *interface_name, bool force_mock, int mock_rate_hz) {
    can_bus_t *bus = (can_bus_t*)calloc(1, sizeof(can_bus_t));
    if (!bus) {
        return NULL;
    }

    if (interface_name && strlen(interface_name) > 0) {
        snprintf(bus->interface, sizeof(bus->interface), "%s", interface_name);
    } else {
        snprintf(bus->interface, sizeof(bus->interface), "vcan0");
    }

    bus->socket_fd = -1;
    bus->mock_rate_hz = (mock_rate_hz > 0) ? mock_rate_hz : 10;
    bus->frame_sequence = 0;
    clock_gettime(CLOCK_MONOTONIC, &bus->start_tp);

    /* Initialize simulation starting state */
    bus->sim_time_sec = 0.0;
    bus->sim_rpm = 850.0;
    bus->sim_speed = 0.0;
    bus->sim_throttle = 15.0;
    bus->sim_coolant = 75.0;
    bus->sim_oil_temp = 70.0;
    bus->sim_voltage = 14.1;
    bus->sim_brake_press = 0.0;
    bus->sim_gear = 1;

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

    /* Fallback to mock generator */
    bus->is_mock = true;
    bus->socket_fd = -1;
    return bus;
}

static void generate_mock_frame(can_bus_t *bus, can_msg_t *msg) {
    double dt = 1.0 / (double)bus->mock_rate_hz;
    bus->sim_time_sec += dt;
    double t = bus->sim_time_sec;

    /* Dynamic vehicle powertrain simulation equations */
    double throttle_wave = 15.0 + 35.0 * (sin(t * 0.35) * 0.5 + 0.5) + 15.0 * (sin(t * 0.8) * 0.5 + 0.5);
    bus->sim_throttle = throttle_wave;

    /* Engine RPM cycles between 850 (idle) and 4800 RPM */
    double rpm_base = 800.0 + (bus->sim_throttle / 65.0) * 3600.0;
    bus->sim_rpm = rpm_base + 50.0 * sin(t * 5.0);
    if (bus->sim_rpm < 750.0) bus->sim_rpm = 750.0;

    /* Transmission gear calculation */
    if (bus->sim_rpm > 3500.0 && bus->sim_gear < 6) {
        bus->sim_gear++;
    } else if (bus->sim_rpm < 1400.0 && bus->sim_gear > 1) {
        bus->sim_gear--;
    }

    /* Vehicle Speed in km/h derived from RPM and gear ratio */
    double speed_calc = (bus->sim_rpm / 3800.0) * (bus->sim_gear * 22.0);
    bus->sim_speed = (speed_calc < 0.0) ? 0.0 : speed_calc;

    /* Thermal warming */
    if (bus->sim_coolant < 90.0) {
        bus->sim_coolant += 0.05;
    }
    bus->sim_coolant += 0.2 * sin(t * 0.1);

    if (bus->sim_oil_temp < 93.0) {
        bus->sim_oil_temp += 0.04;
    }
    bus->sim_oil_temp += 0.15 * cos(t * 0.15);

    /* Alternator voltage with slight electrical load fluctuation */
    bus->sim_voltage = 14.15 + 0.15 * sin(t * 1.5);

    /* Brake application simulation: occasional deceleration brake tap */
    if (fmod(t, 12.0) > 9.5) {
        bus->sim_brake_press = 18.5 + 4.0 * sin(t * 4.0);
    } else {
        bus->sim_brake_press = 0.0;
    }

    /* Common message metadata */
    msg->timestamp_sec = get_monotonic_sec();
    get_current_iso_time(msg->timestamp_iso, sizeof(msg->timestamp_iso));
    msg->is_extended = false;
    msg->is_rtr = false;
    msg->is_error = false;
    memset(msg->data, 0, sizeof(msg->data));

    /* Rotate through sample automotive messages */
    uint32_t step = (uint32_t)(bus->frame_sequence % 4);
    bus->frame_sequence++;

    switch (step) {
        case 0: {
            /* 0x100: Engine_Data (DLC: 8) */
            msg->can_id = 0x100;
            msg->dlc = 8;

            /* EngineRPM: 0|16@1+ (0.25, 0) -> raw = rpm / 0.25 */
            uint16_t raw_rpm = (uint16_t)(bus->sim_rpm / 0.25);
            msg->data[0] = (uint8_t)(raw_rpm & 0xFF);
            msg->data[1] = (uint8_t)((raw_rpm >> 8) & 0xFF);

            /* VehicleSpeed: 16|16@1+ (0.01, 0) -> raw = speed / 0.01 */
            uint16_t raw_speed = (uint16_t)(bus->sim_speed / 0.01);
            msg->data[2] = (uint8_t)(raw_speed & 0xFF);
            msg->data[3] = (uint8_t)((raw_speed >> 8) & 0xFF);

            /* ThrottlePosition: 32|8@1+ (0.392157, 0) -> raw = throttle / 0.392157 */
            uint8_t raw_throttle = (uint8_t)(bus->sim_throttle / 0.392157);
            msg->data[4] = raw_throttle;

            /* EngineOilTemp: 40|8@1+ (1, -40) -> raw = oil_temp - (-40) */
            int raw_oil = (int)(bus->sim_oil_temp + 40.0);
            if (raw_oil < 0) raw_oil = 0;
            if (raw_oil > 255) raw_oil = 255;
            msg->data[5] = (uint8_t)raw_oil;

            msg->data[6] = 0xAA; /* Diagnostic counter */
            msg->data[7] = 0x55; /* Checksum placeholder */
            break;
        }

        case 1: {
            /* 0x101: Engine_Status (DLC: 8) */
            msg->can_id = 0x101;
            msg->dlc = 8;

            /* CoolantTemperature: 0|8@1+ (1, -40) */
            int raw_coolant = (int)(bus->sim_coolant + 40.0);
            if (raw_coolant < 0) raw_coolant = 0;
            if (raw_coolant > 255) raw_coolant = 255;
            msg->data[0] = (uint8_t)raw_coolant;

            /* FuelPressure: 8|8@1+ (4, 0) [380 kPa typical] */
            uint8_t raw_fuel = (uint8_t)(380.0 / 4.0);
            msg->data[1] = raw_fuel;

            /* EngineStatus: 16|2@1+ (1, 0) [1=Running], CheckEngineLight: 18|1@1+ (1, 0) [0=Off] */
            uint8_t engine_status = 1; /* 1 = Running */
            uint8_t mil_lamp = 0;      /* 0 = Off */
            msg->data[2] = (uint8_t)((engine_status & 0x03) | ((mil_lamp & 0x01) << 2));

            /* BatteryVoltage: 24|8@1+ (0.1, 0) */
            uint8_t raw_volt = (uint8_t)(bus->sim_voltage / 0.1);
            msg->data[3] = raw_volt;

            msg->data[4] = 0x00;
            msg->data[5] = 0x00;
            msg->data[6] = 0x00;
            msg->data[7] = 0x00;
            break;
        }

        case 2: {
            /* 0x200: Brake_System (DLC: 6) */
            msg->can_id = 0x200;
            msg->dlc = 6;

            /* BrakePedalPosition: 0|8@1+ (0.392157, 0) */
            double pedal = (bus->sim_brake_press > 0.0) ? 22.0 : 0.0;
            uint8_t raw_pedal = (uint8_t)(pedal / 0.392157);
            msg->data[0] = raw_pedal;

            /* BrakePressure: 8|16@1+ (0.1, 0) */
            uint16_t raw_press = (uint16_t)(bus->sim_brake_press / 0.1);
            msg->data[1] = (uint8_t)(raw_press & 0xFF);
            msg->data[2] = (uint8_t)((raw_press >> 8) & 0xFF);

            /* ABS_Active: 24|1@1+ (1, 0) */
            uint8_t abs_active = 0;
            msg->data[3] = (uint8_t)(abs_active & 0x01);
            msg->data[4] = 0x00;
            msg->data[5] = 0x00;
            break;
        }

        case 3:
        default: {
            /* 0x300: Transmission_Data (DLC: 4) */
            msg->can_id = 0x300;
            msg->dlc = 4;

            /* CurrentGear: 0|4@1+ (1, 0) */
            uint8_t gear = (uint8_t)(bus->sim_gear & 0x0F);
            msg->data[0] = gear;

            /* TransmissionOilTemp: 8|8@1+ (1, -40) [82 degC] */
            int raw_trans_temp = (int)(82.0 + 40.0);
            msg->data[1] = (uint8_t)raw_trans_temp;

            /* ClutchState: 16|2@1+ (1, 0) [2=Closed/Engaged] */
            uint8_t clutch_state = 2;
            msg->data[2] = (uint8_t)(clutch_state & 0x03);
            msg->data[3] = 0x00;
            break;
        }
    }
}

int can_bus_read(can_bus_t *bus, can_msg_t *msg, int timeout_ms) {
    if (!bus || !msg) {
        return -1;
    }

    if (bus->is_mock) {
        /* Mock mode pacing based on configured mock_rate_hz */
        useconds_t sleep_usec = (useconds_t)(1000000 / bus->mock_rate_hz);
        usleep(sleep_usec);
        generate_mock_frame(bus, msg);
        return 1;
    }

#if CAN_HAS_SOCKETCAN
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
        if (errno == EINTR) {
            return 0; /* Interrupted by signal */
        }
        return -1;
    } else if (ret == 0) {
        return 0; /* Timeout */
    }

    struct can_frame frame;
    ssize_t nbytes = read(bus->socket_fd, &frame, sizeof(struct can_frame));
    if (nbytes < (ssize_t)sizeof(struct can_frame)) {
        return -1;
    }

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
#else
    return -1;
#endif
}

bool can_bus_is_mock(const can_bus_t *bus) {
    return bus ? bus->is_mock : false;
}

const char* can_bus_get_interface(const can_bus_t *bus) {
    return bus ? bus->interface : "unknown";
}

void can_bus_close(can_bus_t *bus) {
    if (!bus) return;

#if CAN_HAS_SOCKETCAN
    if (bus->socket_fd >= 0) {
        close(bus->socket_fd);
        bus->socket_fd = -1;
    }
#endif

    free(bus);
}

void can_msg_to_hex_string(const can_msg_t *msg, char *buffer, size_t buf_len) {
    if (!buffer || buf_len == 0) return;
    buffer[0] = '\0';
    if (!msg || msg->dlc == 0) return;

    size_t offset = 0;
    for (uint8_t i = 0; i < msg->dlc && offset + 3 < buf_len; i++) {
        int written = snprintf(buffer + offset, buf_len - offset, "%02X%s",
                               msg->data[i], (i + 1 < msg->dlc) ? " " : "");
        if (written > 0) {
            offset += (size_t)written;
        }
    }
}
