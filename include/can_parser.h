#ifndef CAN_PARSER_H
#define CAN_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__linux__)
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <net/if.h>
    #include <linux/can.h>
    #include <linux/can/raw.h>
    #define CAN_HAS_SOCKETCAN 1
#else
    #define CAN_HAS_SOCKETCAN 0
    /* Fallback definitions for non-Linux compilation */
    typedef uint32_t canid_t;
    #define CAN_EFF_FLAG 0x80000000U
    #define CAN_RTR_FLAG 0x40000000U
    #define CAN_ERR_FLAG 0x20000000U
    #define CAN_SFF_MASK 0x000007FFU
    #define CAN_EFF_MASK 0x1FFFFFFFU
    #define CAN_MAX_DLEN 8

    struct can_frame {
        canid_t can_id;
        uint8_t can_dlc;
        uint8_t __pad;
        uint8_t __res0;
        uint8_t __res1;
        uint8_t data[CAN_MAX_DLEN];
    };
#endif

#define CAN_MAX_DATA_LEN 8

/**
 * @brief High-level CAN message representation
 */
typedef struct {
    uint32_t can_id;          /**< Clean CAN ID (standard 11-bit or extended 29-bit) */
    uint8_t  dlc;             /**< Data length code (0 - 8 bytes) */
    uint8_t  data[CAN_MAX_DATA_LEN]; /**< Raw payload data bytes */
    double   timestamp_sec;   /**< Monotonic time in seconds */
    char     timestamp_iso[64]; /**< ISO-8601 formatted timestamp string */
    bool     is_extended;     /**< True if 29-bit extended frame */
    bool     is_rtr;          /**< True if Remote Transmission Request */
    bool     is_error;        /**< True if error frame */
} can_msg_t;

/**
 * @brief Opaque CAN bus handle
 */
typedef struct can_bus can_bus_t;

/**
 * @brief Initialize CAN bus acquisition (SocketCAN with automatic mock fallback)
 * 
 * @param interface_name SocketCAN interface name (e.g., "vcan0", "can0")
 * @param force_mock If true, forces simulated mock frame generation
 * @param mock_rate_hz Frequency of mock frame generation in Hz (e.g., 10 - 100)
 * @return can_bus_t* Initialized handle, or NULL on memory allocation error
 */
can_bus_t* can_bus_init(const char *interface_name, bool force_mock, int mock_rate_hz);

/**
 * @brief Read the next CAN message from bus (or mock generator)
 * 
 * @param bus Active CAN bus handle
 * @param msg Pointer to message structure to populate
 * @param timeout_ms Read timeout in milliseconds (-1 for blocking, 0 for non-blocking)
 * @return int 1 on successful message received, 0 on timeout, -1 on error
 */
int can_bus_read(can_bus_t *bus, can_msg_t *msg, int timeout_ms);

/**
 * @brief Check if the CAN bus handle is operating in mock mode
 */
bool can_bus_is_mock(const can_bus_t *bus);

/**
 * @brief Get the interface name associated with the bus
 */
const char* can_bus_get_interface(const can_bus_t *bus);

/**
 * @brief Close CAN bus and release all resources
 */
void can_bus_close(can_bus_t *bus);

/**
 * @brief Format raw CAN payload as space-separated hexadecimal bytes
 * 
 * @param msg Source CAN message
 * @param buffer Output destination string buffer
 * @param buf_len Length of output buffer
 */
void can_msg_to_hex_string(const can_msg_t *msg, char *buffer, size_t buf_len);

#endif /* CAN_PARSER_H */
