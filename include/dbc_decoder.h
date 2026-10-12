#ifndef DBC_DECODER_H
#define DBC_DECODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DBC_MAX_SIGNALS_PER_MSG 32
#define DBC_MAX_NAME_LEN 64
#define DBC_MAX_UNIT_LEN 32
#define DBC_MAX_COMMENT_LEN 128

typedef enum {
    CAN_ENDIAN_BIG = 0,    /**< Motorola / Big Endian */
    CAN_ENDIAN_LITTLE = 1  /**< Intel / Little Endian */
} can_endian_t;

typedef enum {
    CAN_SIGN_UNSIGNED = 0, /**< Unsigned integer */
    CAN_SIGN_SIGNED = 1    /**< Signed two's complement integer */
} can_sign_t;

/**
 * @brief Representation of a signal extracted from a DBC definition
 */
typedef struct {
    char         name[DBC_MAX_NAME_LEN];
    uint8_t      start_bit;
    uint8_t      bit_len;
    can_endian_t endian;
    can_sign_t   sign;
    double       scale;
    double       offset;
    double       min;
    double       max;
    char         unit[DBC_MAX_UNIT_LEN];
    char         receiver[DBC_MAX_NAME_LEN];
    char         comment[DBC_MAX_COMMENT_LEN];
} dbc_signal_t;

/**
 * @brief Representation of a CAN message extracted from a DBC definition
 */
typedef struct {
    uint32_t     id;
    char         name[DBC_MAX_NAME_LEN];
    uint8_t      dlc;
    char         transmitter[DBC_MAX_NAME_LEN];
    char         comment[DBC_MAX_COMMENT_LEN];
    size_t       signal_count;
    dbc_signal_t signals[DBC_MAX_SIGNALS_PER_MSG];
} dbc_message_t;

/**
 * @brief In-memory DBC database
 */
typedef struct {
    dbc_message_t *messages;
    size_t         message_count;
    size_t         capacity;
    char           filepath[256];
} dbc_database_t;

/**
 * @brief Decoded signal value result
 */
typedef struct {
    char    signal_name[DBC_MAX_NAME_LEN];
    char    unit[DBC_MAX_UNIT_LEN];
    int64_t raw_value;
    double  physical_value;
    double  min;
    double  max;
    bool    has_range;
    bool    in_range;
} dbc_decoded_signal_t;

/**
 * @brief Decoded message containing all child decoded signals
 */
typedef struct {
    uint32_t             message_id;
    char                 message_name[DBC_MAX_NAME_LEN];
    uint8_t              dlc;
    size_t               signal_count;
    dbc_decoded_signal_t signals[DBC_MAX_SIGNALS_PER_MSG];
} dbc_decoded_message_t;

/**
 * @brief Load and parse a Vector DBC database file
 * 
 * @param filepath Path to the .dbc file
 * @return dbc_database_t* Pointer to allocated database, or NULL on error
 */
dbc_database_t* dbc_load_file(const char *filepath);

/**
 * @brief Free all memory allocated by a DBC database
 */
void dbc_free(dbc_database_t *db);

/**
 * @brief Find a message definition by CAN ID
 * 
 * @param db Database handle
 * @param can_id CAN identifier (standard or extended)
 * @return const dbc_message_t* Pointer to message, or NULL if not defined
 */
const dbc_message_t* dbc_find_message(const dbc_database_t *db, uint32_t can_id);

/**
 * @brief Decode a single signal from raw CAN payload bytes
 * 
 * @param sig Signal definition
 * @param data CAN data payload bytes
 * @param dlc Data length code (number of bytes available)
 * @param raw_out Optional pointer to store extracted integer raw value
 * @return double Scaled physical engineering value (raw * scale + offset)
 */
double dbc_decode_signal(const dbc_signal_t *sig, const uint8_t *data, uint8_t dlc, int64_t *raw_out);

/**
 * @brief Decode all signals in a CAN message
 * 
 * @param msg_def Message definition from DBC
 * @param payload Raw CAN payload bytes
 * @param dlc Payload length
 * @param result Output decoded message structure
 * @return int Number of successfully decoded signals
 */
int dbc_decode_message(const dbc_message_t *msg_def, const uint8_t *payload, uint8_t dlc, dbc_decoded_message_t *result);

/**
 * @brief Encode a physical engineering value into raw CAN payload bytes
 * 
 * @param sig Signal definition
 * @param physical_value Engineering value to encode
 * @param data Target CAN payload bytes (must be at least 8 bytes)
 * @param dlc Maximum payload length
 * @return int 0 on success, -1 on bounds/length error
 */
int dbc_encode_signal(const dbc_signal_t *sig, double physical_value, uint8_t *data, uint8_t dlc);

/**
 * @brief Print a human-readable summary of loaded messages and signals
 */
void dbc_print_summary(const dbc_database_t *db);

#endif /* DBC_DECODER_H */
