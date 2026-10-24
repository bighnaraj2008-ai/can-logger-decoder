#define _GNU_SOURCE
#include "can_core.h"
#include "error_codes.h"
#include "vcan_interface.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef CAN_HAS_SOCKETCAN
#define CAN_HAS_SOCKETCAN VCAN_HAS_SOCKETCAN
#endif

/* Forward declarations of can_bus handle API */
typedef struct can_bus can_bus_t;
can_bus_t *can_bus_init(const char *interface_name, bool force_mock, int mock_rate_hz);
int can_bus_read(can_bus_t *bus, can_msg_t *msg, int timeout_ms);
bool can_bus_is_mock(const can_bus_t *bus);
const char *can_bus_get_interface(const can_bus_t *bus);
void can_bus_close(can_bus_t *bus);
void can_msg_to_hex_string(const can_msg_t *msg, char *buffer, size_t buf_len);
can_error_t can_bus_last_error(const can_bus_t *bus);

#if CAN_HAS_SOCKETCAN
    #include <fcntl.h>
    #include <sys/ioctl.h>
    #include <sys/select.h>
    #include <sys/time.h>
    #include <net/if.h>
    #include <linux/can.h>
    #include <linux/can/raw.h>
#endif

/* ----------------------------------------------------------------------
 * Portable time helpers
 *
 * gmtime_r() is a POSIX function; on MinGW/Windows toolchains that do not
 * expose it without extra feature-test macros, fall back to gmtime() with a
 * structure copy. This keeps the module compiling cleanly on every host.
 * ---------------------------------------------------------------------- */
#if defined(_WIN32)
static struct tm *can_gmtime_r(const time_t *timep, struct tm *result) {
    struct tm *tmp = gmtime(timep);
    if (tmp != NULL) {
        *result = *tmp;
    }
    return tmp;
}
#else
#define can_gmtime_r gmtime_r
#endif

static double get_monotonic_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
}

static void get_current_iso_time(char *buf, size_t len) {
    struct timespec ts;
    struct tm tm_info;
    unsigned int ms;

    clock_gettime(CLOCK_REALTIME, &ts);
    if (can_gmtime_r(&ts.tv_sec, &tm_info) == NULL) {
        memset(&tm_info, 0, sizeof(tm_info));
    }
    ms = (unsigned int)(ts.tv_nsec / 1000000L);
    snprintf(buf, len, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
             (unsigned int)(tm_info.tm_year + 1900),
             (unsigned int)(tm_info.tm_mon + 1),
             (unsigned int)tm_info.tm_mday,
             (unsigned int)tm_info.tm_hour,
             (unsigned int)tm_info.tm_min,
             (unsigned int)tm_info.tm_sec,
             ms);
}

/* ----------------------------------------------------------------------
 * Opaque bus handle
 * ---------------------------------------------------------------------- */
struct can_bus {
    char interface[64];
    vcan_socket_t sock;
    bool is_mock;
    int mock_rate_hz;
    uint64_t frame_sequence;
    struct timespec start_tp;

    /* Mock generator state */
    double sim_time_sec;
    double sim_rpm;
    double sim_speed;
    double sim_throttle;
    double sim_coolant;
    double sim_oil_temp;
    double sim_voltage;
    double sim_brake_press;
    int    sim_gear;

    /* Last error detail */
    can_error_t last_error;
};

/* ----------------------------------------------------------------------
 * Wire frame <-> host message conversion
 * ---------------------------------------------------------------------- */
#if CAN_HAS_SOCKETCAN
static void frame_to_msg(const can_frame_t *frame, can_msg_t *msg) {
    can_msg_clear(msg);
    msg->can_id = can_id_clean(frame->can_id);
    msg->dlc = (frame->can_dlc > CAN_DLC_MAX) ? CAN_DLC_MAX : frame->can_dlc;
    if (msg->dlc > 0) {
        memcpy(msg->data, frame->data, (size_t)msg->dlc);
    }
    msg->is_extended = frame->is_extended;
    msg->is_rtr = frame->is_rtr;
    msg->is_error = frame->is_error;
    msg->checksum = frame->checksum;
    msg->checksum_valid = frame->checksum_valid;

    msg->timestamp_sec = get_monotonic_sec();
    get_current_iso_time(msg->timestamp_iso, sizeof(msg->timestamp_iso));
}
#endif /* CAN_HAS_SOCKETCAN */

/* ----------------------------------------------------------------------
 * Mock telemetry generator (fallback when no real CAN interface exists)
 * ---------------------------------------------------------------------- */
static void generate_mock_frame(can_bus_t *bus, can_msg_t *msg) {
    double dt = 1.0 / (double)bus->mock_rate_hz;
    bus->sim_time_sec += dt;
    double t = bus->sim_time_sec;

    /* Dynamic vehicle powertrain simulation equations */
    double throttle_wave = 15.0 + 35.0 * (sin(t * 0.35) * 0.5 + 0.5) +
                           15.0 * (sin(t * 0.8) * 0.5 + 0.5);
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
    uint32_t step = (uint32_t)(bus->frame_sequence % 4u);
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

            /* ThrottlePosition: 32|8@1+ (0.392157, 0) */
            uint8_t raw_throttle = (uint8_t)(bus->sim_throttle / 0.392157);
            msg->data[4] = raw_throttle;

            /* EngineOilTemp: 40|8@1+ (1, -40) -> raw = oil_temp + 40 */
            int raw_oil = (int)(bus->sim_oil_temp + 40.0);
            if (raw_oil < 0) raw_oil = 0;
            if (raw_oil > 255) raw_oil = 255;
            msg->data[5] = (uint8_t)raw_oil;

            msg->data[6] = 0xAA; /* Diagnostic counter */
            /* Byte 7 carries a CRC-8 checksum over bytes 0-6 */
            can_checksum_append(msg->data, 7u);
            msg->checksum = msg->data[7];
            msg->checksum_valid = true;
            break;
        }

        case 1: {
            /* 0x101: Engine_Status (DLC: 8) */
            msg->can_id = 0x101;
            msg->dlc = 8;

            int raw_coolant = (int)(bus->sim_coolant + 40.0);
            if (raw_coolant < 0) raw_coolant = 0;
            if (raw_coolant > 255) raw_coolant = 255;
            msg->data[0] = (uint8_t)raw_coolant;

            /* FuelPressure: 8|8@1+ (4, 0) [380 kPa typical] */
            msg->data[1] = (uint8_t)(380.0 / 4.0);

            /* EngineStatus: 16|2@1+ [1=Running], CheckEngineLight: 18|1@1+ [0=Off] */
            uint8_t engine_status = 1;  /* 1 = Running */
            uint8_t mil_lamp = 0;       /* 0 = Off */
            msg->data[2] = (uint8_t)((engine_status & 0x03) | ((mil_lamp & 0x01) << 2));

            /* BatteryVoltage: 24|8@1+ (0.1, 0) */
            msg->data[3] = (uint8_t)(bus->sim_voltage / 0.1);

            msg->data[4] = 0x00;
            msg->data[5] = 0x00;
            msg->data[6] = 0x00;
            can_checksum_append(msg->data, 7u);
            msg->checksum = msg->data[7];
            msg->checksum_valid = true;
            break;
        }

        case 2: {
            /* 0x200: Brake_System (DLC: 6) */
            msg->can_id = 0x200;
            msg->dlc = 6;

            /* BrakePedalPosition: 0|8@1+ (0.392157, 0) */
            double pedal = (bus->sim_brake_press > 0.0) ? 22.0 : 0.0;
            msg->data[0] = (uint8_t)(pedal / 0.392157);

            /* BrakePressure: 8|16@1+ (0.1, 0) */
            uint16_t raw_press = (uint16_t)(bus->sim_brake_press / 0.1);
            msg->data[1] = (uint8_t)(raw_press & 0xFF);
            msg->data[2] = (uint8_t)((raw_press >> 8) & 0xFF);

            /* ABS_Active: 24|1@1+ (1, 0) */
            msg->data[3] = 0x00;
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
            msg->data[0] = (uint8_t)(bus->sim_gear & 0x0F);

            /* TransmissionOilTemp: 8|8@1+ (1, -40) */
            msg->data[1] = (uint8_t)(82.0 + 40.0);

            /* ClutchState: 16|2@1+ (1, 0) [2=Closed/Engaged] */
            msg->data[2] = (uint8_t)(2 & 0x03);
            msg->data[3] = 0x00;
            break;
        }
    }
}

/* ----------------------------------------------------------------------
 * Opaque bus handle
 * ---------------------------------------------------------------------- */

can_bus_t *can_bus_init(const char *interface_name, bool force_mock, int mock_rate_hz) {
    can_bus_t *bus = (can_bus_t *)calloc(1, sizeof(can_bus_t));
    if (bus == NULL) {
        return NULL;
    }

    if (interface_name != NULL && interface_name[0] != '\0') {
        snprintf(bus->interface, sizeof(bus->interface), "%s", interface_name);
    } else {
        snprintf(bus->interface, sizeof(bus->interface), "vcan0");
    }

    bus->sock.fd = -1;
    bus->sock.nonblock = false;
    bus->sock.last_err = (int)CAN_OK;
    bus->mock_rate_hz = (mock_rate_hz > 0) ? mock_rate_hz : 10;
    bus->frame_sequence = 0;
    clock_gettime(CLOCK_MONOTONIC, &bus->start_tp);

    /* Mock simulation starting state */
    bus->sim_time_sec = 0.0;
    bus->sim_rpm = 850.0;
    bus->sim_speed = 0.0;
    bus->sim_throttle = 15.0;
    bus->sim_coolant = 75.0;
    bus->sim_oil_temp = 70.0;
    bus->sim_voltage = 14.1;
    bus->sim_brake_press = 0.0;
    bus->sim_gear = 1;

    bus->last_error = CAN_OK;
    (void)force_mock; /* used on Linux SocketCAN path; intentional no-op here */

#if CAN_HAS_SOCKETCAN
    if (!force_mock) {
        int fd = vcan_socket_open();
        if (fd >= 0) {
            bus->sock.fd = fd;
            if (vcan_socket_bind(&bus->sock, bus->interface) == 0) {
                bus->sock.nonblock = true;
                bus->is_mock = false;
                bus->last_error = CAN_OK;
                return bus;
            }
            vcan_socket_close(&bus->sock);
        }
        /* Fall through to mock on any interface failure */
    }
#endif

    /* Fallback to mock generator */
    bus->is_mock = true;
    bus->last_error = CAN_OK;
    return bus;
}

int can_bus_read(can_bus_t *bus, can_msg_t *msg, int timeout_ms) {
    if (bus == NULL || msg == NULL) {
        return -1;
    }
    (void)timeout_ms; /* handled by the mock / socket layer below */

    if (bus->is_mock) {
        useconds_t sleep_usec = (useconds_t)(1000000 / bus->mock_rate_hz);
        usleep(sleep_usec);
        generate_mock_frame(bus, msg);
        bus->last_error = CAN_OK;
        return 1;
    }

#if CAN_HAS_SOCKETCAN
    struct timeval tv;
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(bus->sock.fd, &read_fds);

    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (long)(timeout_ms % 1000) * 1000L;
    }

    int ret = select(bus->sock.fd + 1, &read_fds, NULL, NULL,
                     (timeout_ms >= 0) ? &tv : NULL);
    if (ret < 0) {
        bus->last_error = CAN_ERR_IO;
        return -1;
    } else if (ret == 0) {
        bus->last_error = CAN_OK;
        return 0; /* timeout */
    }

    /* Read the native wire frame from the socket */
    struct can_frame wire;
    ssize_t nbytes = read(bus->sock.fd, &wire, sizeof(wire));
    if (nbytes < (ssize_t)sizeof(wire)) {
        bus->last_error = CAN_ERR_IO;
        return -1;
    }

    can_frame_t cf;
    can_frame_init(&cf);
    cf.can_id = wire.can_id;
    cf.can_dlc = wire.can_dlc;
    if (cf.can_dlc > CAN_FRAME_DATA_LEN) cf.can_dlc = CAN_FRAME_DATA_LEN;
    memcpy(cf.data, wire.data, (size_t)cf.can_dlc);
    cf.is_extended = ((wire.can_id & CAN_EFF_FLAG) != 0u);
    cf.is_rtr = ((wire.can_id & CAN_RTR_FLAG) != 0u);
    cf.is_error = ((wire.can_id & CAN_ERR_FLAG) != 0u);

    if (!can_frame_is_valid(&cf)) {
        bus->last_error = CAN_ERR_INVALID_DLC;
        return -1;
    }

    frame_to_msg(&cf, msg);
    bus->last_error = CAN_OK;
    return 1;
#else
    bus->last_error = CAN_ERR_UNSUPPORTED;
    return -1;
#endif /* CAN_HAS_SOCKETCAN */
}

bool can_bus_is_mock(const can_bus_t *bus) {
    return bus ? bus->is_mock : false;
}

const char *can_bus_get_interface(const can_bus_t *bus) {
    return bus ? bus->interface : "unknown";
}

void can_bus_close(can_bus_t *bus) {
    if (bus == NULL) return;

#if CAN_HAS_SOCKETCAN
    if (bus->sock.fd >= 0) {
        vcan_socket_close(&bus->sock);
        bus->sock.fd = -1;
    }
#endif

    free(bus);
}

can_error_t can_bus_last_error(const can_bus_t *bus) {
    return bus ? bus->last_error : CAN_ERR_NOT_INIT;
}

void can_msg_to_hex_string(const can_msg_t *msg, char *buffer, size_t buf_len) {
    if (buffer == NULL || buf_len == 0) return;
    buffer[0] = '\0';
    if (msg == NULL || msg->dlc == 0) return;

    size_t offset = 0;
    for (uint8_t i = 0; i < msg->dlc && offset + 3 < buf_len; i++) {
        int written = snprintf(buffer + offset, buf_len - offset, "%02X%s",
                               msg->data[i], (i + 1 < msg->dlc) ? " " : "");
        if (written > 0) {
            offset += (size_t)written;
        }
    }
}
