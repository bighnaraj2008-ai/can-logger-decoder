#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../include/dbc_decoder.h"
#include "../include/can_parser.h"

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while(0)

static int test_dbc_parser(void) {
    printf("\n=== Running DBC Parser Test ===\n");
    dbc_database_t *db = dbc_load_file("sample.dbc");
    TEST_ASSERT(db != NULL, "DBC database loaded successfully");
    TEST_ASSERT(db->message_count >= 4, "Found at least 4 messages in sample.dbc");

    /* Verify Engine_Data (0x100 = 256) */
    const dbc_message_t *msg_engine = dbc_find_message(db, 0x100);
    TEST_ASSERT(msg_engine != NULL, "Found Engine_Data (0x100)");
    TEST_ASSERT(strcmp(msg_engine->name, "Engine_Data") == 0, "Message name is Engine_Data");
    TEST_ASSERT(msg_engine->dlc == 8, "Engine_Data DLC is 8");
    TEST_ASSERT(msg_engine->signal_count == 4, "Engine_Data has 4 signals");

    /* Verify EngineRPM signal */
    const dbc_signal_t *sig_rpm = &msg_engine->signals[0];
    TEST_ASSERT(strcmp(sig_rpm->name, "EngineRPM") == 0, "Signal 0 name is EngineRPM");
    TEST_ASSERT(sig_rpm->start_bit == 0, "EngineRPM start bit is 0");
    TEST_ASSERT(sig_rpm->bit_len == 16, "EngineRPM bit length is 16");
    TEST_ASSERT(fabs(sig_rpm->scale - 0.25) < 1e-6, "EngineRPM scale is 0.25");
    TEST_ASSERT(fabs(sig_rpm->offset - 0.0) < 1e-6, "EngineRPM offset is 0.0");
    TEST_ASSERT(strcmp(sig_rpm->unit, "rpm") == 0, "EngineRPM unit is 'rpm'");

    /* Verify CoolantTemperature in Engine_Status (0x101 = 257) */
    const dbc_message_t *msg_status = dbc_find_message(db, 0x101);
    TEST_ASSERT(msg_status != NULL, "Found Engine_Status (0x101)");
    const dbc_signal_t *sig_coolant = &msg_status->signals[0];
    TEST_ASSERT(strcmp(sig_coolant->name, "CoolantTemperature") == 0, "CoolantTemperature signal found");
    TEST_ASSERT(fabs(sig_coolant->scale - 1.0) < 1e-6, "Coolant scale is 1.0");
    TEST_ASSERT(fabs(sig_coolant->offset - (-40.0)) < 1e-6, "Coolant offset is -40.0");

    dbc_free(db);
    return 0;
}

static int test_signal_decoding_math(void) {
    printf("\n=== Running Signal Decoding Math Test ===\n");
    dbc_database_t *db = dbc_load_file("sample.dbc");
    TEST_ASSERT(db != NULL, "DBC loaded for math test");

    const dbc_message_t *msg_engine = dbc_find_message(db, 0x100);
    TEST_ASSERT(msg_engine != NULL, "Found Engine_Data definition");

    /* Prepare known payload:
     * EngineRPM = 2500 rpm -> raw = 2500 / 0.25 = 10000 = 0x2710 -> Byte0: 0x10, Byte1: 0x27
     * VehicleSpeed = 80.50 km/h -> raw = 80.50 / 0.01 = 8050 = 0x1F72 -> Byte2: 0x72, Byte3: 0x1F
     * ThrottlePosition = 50.0% -> raw = round(50.0 / 0.392157) = 127 = 0x7F -> Byte4: 0x7F
     * EngineOilTemp = 90.0 degC -> raw = 90.0 - (-40) = 130 = 0x82 -> Byte5: 0x82
     */
    uint8_t payload[8] = { 0x10, 0x27, 0x72, 0x1F, 0x7F, 0x82, 0x00, 0x00 };

    dbc_decoded_message_t decoded;
    int sig_count = dbc_decode_message(msg_engine, payload, 8, &decoded);
    TEST_ASSERT(sig_count == 4, "Decoded 4 signals");

    /* Check EngineRPM */
    TEST_ASSERT(decoded.signals[0].raw_value == 10000, "Raw EngineRPM value is exactly 10000");
    TEST_ASSERT(fabs(decoded.signals[0].physical_value - 2500.0) < 1e-3, "Physical EngineRPM is 2500.0 rpm");
    TEST_ASSERT(decoded.signals[0].in_range == true, "EngineRPM is within valid range");

    /* Check VehicleSpeed */
    TEST_ASSERT(decoded.signals[1].raw_value == 8050, "Raw VehicleSpeed value is 8050");
    TEST_ASSERT(fabs(decoded.signals[1].physical_value - 80.50) < 1e-3, "Physical VehicleSpeed is 80.50 km/h");

    /* Check ThrottlePosition */
    TEST_ASSERT(decoded.signals[2].raw_value == 127, "Raw ThrottlePosition value is 127");
    TEST_ASSERT(fabs(decoded.signals[2].physical_value - 49.8039) < 0.1, "Physical ThrottlePosition is ~49.8%");

    /* Check EngineOilTemp */
    TEST_ASSERT(decoded.signals[3].raw_value == 130, "Raw EngineOilTemp value is 130");
    TEST_ASSERT(fabs(decoded.signals[3].physical_value - 90.0) < 1e-3, "Physical EngineOilTemp is 90.0 degC");

    dbc_free(db);
    return 0;
}

static int test_mock_bus(void) {
    printf("\n=== Running Mock Bus Acquisition Test ===\n");
    can_bus_t *bus = can_bus_init("mock_virtual", true, 100);
    TEST_ASSERT(bus != NULL, "Initialized mock bus handle");
    TEST_ASSERT(can_bus_is_mock(bus) == true, "Bus confirmed in mock mode");

    can_msg_t msg;
    int res = can_bus_read(bus, &msg, 100);
    TEST_ASSERT(res == 1, "Successfully read frame 1 from mock bus");
    TEST_ASSERT(msg.dlc > 0 && msg.dlc <= 8, "Frame DLC valid (1-8)");
    TEST_ASSERT(msg.timestamp_sec > 0.0, "Timestamp is positive monotonic time");
    TEST_ASSERT(strlen(msg.timestamp_iso) > 10, "ISO timestamp generated");

    char hex_str[32];
    can_msg_to_hex_string(&msg, hex_str, sizeof(hex_str));
    TEST_ASSERT(strlen(hex_str) > 0, "Formatted hex payload non-empty");

    can_bus_close(bus);
    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER UNIT TEST SUITE               \n");
    printf("====================================================\n");

    int failed = 0;
    failed += test_dbc_parser();
    failed += test_signal_decoding_math();
    failed += test_mock_bus();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL TESTS PASSED SUCCESSFULLY!                    \n");
    } else {
        printf(" SOME TESTS FAILED: %d failure(s)                   \n", failed);
    }
    printf("====================================================\n");

    return failed;
}
