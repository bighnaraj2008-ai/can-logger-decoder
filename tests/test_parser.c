#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "can_core.h"
#include "can_filters.h"
#include "error_codes.h"
#include "vcan_interface.h"

typedef struct can_bus can_bus_t;
can_bus_t *can_bus_init(const char *interface_name, bool force_mock, int mock_rate_hz);
int can_bus_read(can_bus_t *bus, can_msg_t *msg, int timeout_ms);
bool can_bus_is_mock(const can_bus_t *bus);
void can_bus_close(can_bus_t *bus);
void can_msg_to_hex_string(const can_msg_t *msg, char *buffer, size_t buf_len);

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

static int test_dlc_bounds(void) {
    printf("\n=== Running DLC Bounds Check Tests ===\n");

    for (uint8_t d = 0; d <= CAN_DLC_MAX; d++) {
        TEST_ASSERT(can_dlc_is_valid(d), "DLC within [0..8] must be valid");
    }
    TEST_ASSERT(!can_dlc_is_valid(9), "DLC 9 must be invalid");
    TEST_ASSERT(!can_dlc_is_valid(255), "DLC 255 must be invalid");
    TEST_ASSERT(!can_dlc_is_valid(15), "DLC 15 must be invalid for classical CAN");

    TEST_ASSERT(can_dlc_to_bytes(8) == 8, "can_dlc_to_bytes(8) == 8");
    TEST_ASSERT(can_dlc_to_bytes(0) == 0, "can_dlc_to_bytes(0) == 0");
    TEST_ASSERT(can_dlc_to_bytes(9) == CAN_ERR_INVALID_DLC,
                "can_dlc_to_bytes(9) must report invalid DLC");

    TEST_ASSERT(can_bytes_to_dlc(8) == 8, "can_bytes_to_dlc(8) == 8");
    TEST_ASSERT(can_bytes_to_dlc(0) == 0, "can_bytes_to_dlc(0) == 0");
    TEST_ASSERT(can_bytes_to_dlc(9) < 0, "can_bytes_to_dlc(9) must fail");

    /* ISO 15765-2 CAN FD mapping spot check */
    TEST_ASSERT(can_dlc_to_length(0) == 0, "FD length(0) == 0");
    TEST_ASSERT(can_dlc_to_length(8) == 8, "FD length(8) == 8");
    TEST_ASSERT(can_dlc_to_length(12) == 24, "FD length(12) == 24");
    TEST_ASSERT(can_dlc_to_length(14) == 48, "FD length(14) == 48");
    TEST_ASSERT(can_dlc_to_length(15) == 64, "FD length(15) == 64");

    return 0;
}

static int test_identifier_masking(void) {
    printf("\n=== Running CAN Identifier Masking Tests ===\n");

    uint32_t ext = CAN_EFF_FLAG | 0x123u;
    TEST_ASSERT(can_id_is_extended(ext), "Extended flag must be detected");
    TEST_ASSERT(!can_id_is_extended(0x100u), "Standard id must not be extended");
    TEST_ASSERT(can_id_clean(ext) == 0x123u,
                "can_id_clean must strip flag bits");
    TEST_ASSERT(can_id_clean(0x100u) == 0x100u, "Clean id passes through");
    TEST_ASSERT(can_id_clean(CAN_ERR_FLAG | 0x7FFu) == 0x7FFu,
                "Error flag must be stripped");

    return 0;
}

static int test_frame_wire_roundtrip(void) {
    printf("\n=== Running Frame Wire Format Round-trip Tests ===\n");

    can_frame_t src;
    can_frame_init(&src);
    src.can_id = CAN_EFF_FLAG | 0x1ABCDu;
    src.can_dlc = 8;
    for (uint8_t i = 0; i < src.can_dlc; i++) src.data[i] = (uint8_t)(i * 0x11u);
    src.is_extended = true;
    src.is_rtr = false;
    src.is_error = false;

    uint8_t wire[64];
    size_t wire_len = sizeof(wire);
    TEST_ASSERT(can_frame_to_wire(&src, wire, &wire_len) == CAN_OK,
                "can_frame_to_wire must succeed");
    TEST_ASSERT(wire_len == 13u, "Wire length = 4 id + 1 dlc + 8 data");
    TEST_ASSERT(wire[0] == 0x80u && wire[1] == 0x01u && wire[2] == 0xABu &&
                wire[3] == 0xCDu, "Identifier bytes include EFF flag, big-endian");
    TEST_ASSERT(wire[4] == 8u, "DLC byte must be 8");

    can_frame_t dst;
    can_frame_init(&dst);
    TEST_ASSERT(can_frame_from_wire(wire, wire_len, &dst) == CAN_OK,
                "can_frame_from_wire must succeed");
    TEST_ASSERT(dst.can_id == src.can_id, "Round-trip id must match");
    TEST_ASSERT(dst.can_dlc == src.can_dlc, "Round-trip dlc must match");
    TEST_ASSERT(memcmp(dst.data, src.data, src.can_dlc) == 0,
                "Round-trip payload must match");
    TEST_ASSERT(dst.is_extended == true, "Extended flag must survive");
    TEST_ASSERT(can_frame_is_valid(&dst), "Round-tripped frame must be valid");

    /* Truncated wire must fail */
    TEST_ASSERT(can_frame_from_wire(wire, 3, &dst) == CAN_ERR_IO,
                "Truncated wire header must fail");
    TEST_ASSERT(can_frame_from_wire(NULL, 10, &dst) == CAN_ERR_INVALID_ARG,
                "NULL wire must fail");

    /* Invalid DLC over the wire */
    uint8_t bad_wire[6] = { 0x00, 0x00, 0x00, 0x10, 9, 0x00 };
    TEST_ASSERT(can_frame_from_wire(bad_wire, 6, &dst) == CAN_ERR_INVALID_DLC,
                "Wire DLC 9 must be rejected");

    return 0;
}

static int test_checksum_utilities(void) {
    printf("\n=== Running Payload Checksum Utility Tests ===\n");

    uint8_t payload[7] = { 0x10u, 0x27u, 0x72u, 0x1Fu, 0x7Fu, 0x82u, 0x00u };
    uint8_t crc = can_checksum_compute(payload, 6u);
    TEST_ASSERT(crc != 0u, "CRC-8 over sample payload must be non-zero");

    /* Append and validate round-trip */
    uint8_t with_crc[8];
    memcpy(with_crc, payload, 7u);
    can_checksum_append(with_crc, 7u);
    TEST_ASSERT(with_crc[7] == can_checksum_compute(with_crc, 7u),
                "Appended checksum must equal CRC of the 7 bytes");
    TEST_ASSERT(can_checksum_validate(with_crc, 7u, with_crc[7]),
                "Checksum validation must pass for intact payload");

    /* Single-bit corruption must be detected */
    uint8_t corrupted[8];
    memcpy(corrupted, with_crc, 8u);
    corrupted[0] ^= 0xFFu;
    TEST_ASSERT(!can_checksum_validate(corrupted, 7u, corrupted[7]),
                "Checksum validation must fail on corrupted payload");

    /* Empty buffer CRC is zero and validates against zero */
    TEST_ASSERT(can_checksum_compute(NULL, 0u) == 0u,
                "CRC-8 over empty buffer must be zero");
    TEST_ASSERT(can_checksum_validate(payload, 0u, 0u),
                "Zero-length validation against zero checksum passes");

    return 0;
}

static int test_mock_bus_checksum(void) {
    printf("\n=== Running Mock Bus Checksum Tests ===\n");

    can_bus_t *bus = can_bus_init("mock_parser_test", true, 1000);
    TEST_ASSERT(bus != NULL, "Mock bus handle initialized");
    TEST_ASSERT(can_bus_is_mock(bus), "Bus is in mock mode");

    can_msg_t msg;
    int res = can_bus_read(bus, &msg, 100);
    TEST_ASSERT(res == 1, "Successfully read mock frame");
    TEST_ASSERT(can_dlc_is_valid(msg.dlc), "Mock frame DLC is valid");
    TEST_ASSERT(msg.checksum_valid, "Mock frame carries valid checksum");
    TEST_ASSERT(msg.checksum == can_checksum_compute(msg.data, msg.dlc - 1u),
                "Checksum byte matches CRC-8 of preceding payload");

    char hex[64];
    can_msg_to_hex_string(&msg, hex, sizeof(hex));
    TEST_ASSERT(strlen(hex) > 0, "Hex payload formatted");

    can_bus_close(bus);
    return 0;
}

static int test_acceptance_filters(void) {
    printf("\n=== Running Acceptance Filter Tests ===\n");

    can_filter_set_t set;
    can_filter_set_init(&set);
    TEST_ASSERT(set.count == 0, "Filter set starts empty");
    TEST_ASSERT(set.promiscuous == false, "Not promiscuous by default");

    can_frame_t frame;
    can_frame_init(&frame);
    frame.can_id = 0x100u;
    frame.can_dlc = 0;

    TEST_ASSERT(!can_filter_match(&set, &frame), "Empty set rejects everything");
    TEST_ASSERT(can_filter_route(&set, &frame) == -1, "Empty set routes nowhere");

    /* Exact match on 11-bit id */
    can_filter_set_add(&set, 0x100u, CAN_SFF_MASK, CAN_FILTER_MODE_STANDARD);
    TEST_ASSERT(set.count == 1, "One filter registered");
    TEST_ASSERT(can_filter_match(&set, &frame), "Exact id matches");
    TEST_ASSERT(can_filter_route(&set, &frame) == 0, "Route returns filter 0");

    frame.can_id = 0x101u;
    TEST_ASSERT(!can_filter_match(&set, &frame), "Non-matching id rejected");

    /* Masked match: mask 0x700 accepts 0x100-0x1FF */
    can_filter_set_clear(&set);
    can_filter_set_add(&set, 0x100u, 0x700u, CAN_FILTER_MODE_STANDARD);
    frame.can_id = 0x100u;
    TEST_ASSERT(can_filter_match(&set, &frame), "0x100 matches masked filter");
    frame.can_id = 0x180u;
    TEST_ASSERT(can_filter_match(&set, &frame), "0x180 matches masked filter");
    frame.can_id = 0x200u;
    TEST_ASSERT(!can_filter_match(&set, &frame), "0x200 rejected by mask");

    /* Extended format separation */
    can_filter_set_clear(&set);
    can_filter_set_add(&set, 0x123u, CAN_EFF_MASK, CAN_FILTER_MODE_EXTENDED);
    frame.can_id = CAN_EFF_FLAG | 0x123u;
    TEST_ASSERT(can_filter_match(&set, &frame), "Extended id matches extended filter");
    frame.can_id = 0x123u;
    TEST_ASSERT(!can_filter_match(&set, &frame),
                "Standard id rejected by extended-only filter");

    /* Dual mode accepts both formats with the same base identifier */
    can_filter_set_clear(&set);
    can_filter_set_add(&set, 0x100u, CAN_SFF_MASK, CAN_FILTER_MODE_DUAL);
    frame.can_id = 0x100u;
    TEST_ASSERT(can_filter_match(&set, &frame), "Standard id matches dual filter");
    frame.can_id = CAN_EFF_FLAG | 0x100u;
    TEST_ASSERT(can_filter_match(&set, &frame),
                "Extended id with same base matches dual filter");
    TEST_ASSERT(can_filter_route(&set, &frame) == 0, "Dual filter routes extended frame");

    /* Promiscuous mode accepts everything */
    can_filter_set_clear(&set);
    set.promiscuous = true;
    frame.can_id = 0xDEADu;
    TEST_ASSERT(can_filter_match(&set, &frame), "Promiscuous accepts unknown id");
    TEST_ASSERT(can_filter_route(&set, &frame) == 0, "Promiscuous routes to 0");

    /* Disabled entries are skipped */
    can_filter_set_clear(&set);
    can_filter_set_add(&set, 0x100u, CAN_SFF_MASK, CAN_FILTER_MODE_STANDARD);
    set.filters[0].enabled = false;
    TEST_ASSERT(!can_filter_match(&set, &frame), "Disabled filter ignored");

    return 0;
}

static int test_vcan_route_frame(void) {
    printf("\n=== Running vcan Frame Routing Tests ===\n");

    can_filter_set_t set;
    can_filter_set_init(&set);
    can_filter_set_add(&set, 0x200u, CAN_SFF_MASK, CAN_FILTER_MODE_STANDARD);

    can_frame_t frame;
    can_frame_init(&frame);
    frame.can_id = 0x200u;
    frame.can_dlc = 0;

    TEST_ASSERT(vcan_route_frame(&frame, &set) == 0, "vcan_route_frame matches filter");
    frame.can_id = 0x300u;
    TEST_ASSERT(vcan_route_frame(&frame, &set) == -1, "vcan_route_frame rejects");
    TEST_ASSERT(vcan_route_frame(NULL, &set) == -1, "NULL frame rejected");
    TEST_ASSERT(vcan_route_frame(&frame, NULL) == -1, "NULL filter set rejected");

    return 0;
}

int main(void) {
    printf("====================================================\n");
    printf(" CAN PARSER & FRAME LAYER UNIT TEST SUITE           \n");
    printf("====================================================\n");

    int failed = 0;
    failed += test_dlc_bounds();
    failed += test_identifier_masking();
    failed += test_frame_wire_roundtrip();
    failed += test_checksum_utilities();
    failed += test_mock_bus_checksum();
    failed += test_acceptance_filters();
    failed += test_vcan_route_frame();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL PARSER TESTS PASSED SUCCESSFULLY!             \n");
    } else {
        printf(" PARSER TEST FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
