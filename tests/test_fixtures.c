#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"
#include "test_fixtures.h"

/* Forward declarations for functions without public headers */
can_error_t can_validate_frame_integrity(const can_frame_t *frame);
bool can_checksum_validate(const uint8_t *data, size_t len, uint8_t checksum);

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } else { \
        printf("  [PASS] %s\n", msg); \
    } \
} while (0)

/* ----------------------------------------------------------------------
 * Expected raw (valid) frames, in file order.
 * Wire layout: [id_b3][id_b2][id_b1][id_b0][dlc][data...]
 * ---------------------------------------------------------------------- */
static const fixture_expected_frame_t expected_raw_frames[] = {
    { 0x00000100u, 8, false, false, false,
      { 0x10, 0x27, 0x72, 0x1F, 0x7F, 0x82, 0xAA, 0x55 } }, /* 0x100 Engine_Data  */
    { 0x00000101u, 8, false, false, false,
      { 0x80, 0x5F, 0x01, 0x8D, 0x00, 0x00, 0x00, 0x00 } }, /* 0x101 Engine_Status*/
    { 0x00000200u, 6, false, false, false,
      { 0x40, 0xC8, 0x00, 0x00, 0x00, 0x00 } },              /* 0x200 Brake_System */
    { 0x00000300u, 4, false, false, false,
      { 0x01, 0x52, 0x02, 0x00 } },                          /* 0x300 Transmission   */
    { CAN_EFF_FLAG | 0x18DAF110u, 8, true, false, false,
      { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04 } },  /* 0x18DAF110 extended */
    { CAN_EFF_FLAG | 0x1FFFFFFFu, 0, true, false, false, { 0 } }, /* max ext id   */
    { CAN_RTR_FLAG | 0x00000100u, 0, false, true, false, { 0 } }, /* RTR frame  */
    { CAN_ERR_FLAG | 0x00000100u, 0, false, false, true, { 0 } }, /* error frame*/
};
#define EXPECTED_RAW_COUNT (sizeof(expected_raw_frames) / sizeof(expected_raw_frames[0]))

/* ----------------------------------------------------------------------
 * Expected extended frames
 * ---------------------------------------------------------------------- */
static const fixture_expected_frame_t expected_extended_frames[] = {
    { CAN_EFF_FLAG | 0x18DAF110u, 8, true, false, false,
      { 0x02, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { CAN_EFF_FLAG | 0x18DA10F1u, 8, true, false, false,
      { 0x06, 0x50, 0x01, 0x00, 0x32, 0x01, 0xF4, 0xAA } },
    { CAN_EFF_FLAG | 0x0CF00400u, 8, true, false, false,
      { 0xF0, 0x7D, 0x84, 0x23, 0x01, 0xF0, 0x7D, 0x84 } },
    { CAN_EFF_FLAG | 0x18FEF200u, 8, true, false, false,
      { 0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x70, 0x81 } },
    { CAN_EFF_FLAG | 0x1FFFFFFFu, 0, true, false, false, { 0 } },
};
#define EXPECTED_EXT_COUNT (sizeof(expected_extended_frames) / sizeof(expected_extended_frames[0]))

/* ----------------------------------------------------------------------
 * Expected corrupted-frame outcomes, in file order.
 * ---------------------------------------------------------------------- */
static const fixture_expected_corrupt_t expected_corrupt[] = {
    { CAN_ERR_IO,            false, CAN_OK,         false, false, false }, /* 1 truncated hdr      */
    { CAN_ERR_INVALID_DLC,   false, CAN_OK,         false, false, false }, /* 2 dlc 9              */
    { CAN_ERR_INVALID_DLC,   false, CAN_OK,         false, false, false }, /* 3 dlc 15             */
    { CAN_ERR_IO,            false, CAN_OK,         false, false, false }, /* 4 truncated payload  */
    { CAN_OK,                true,  CAN_OK,         true,  false, false }, /* 5 high id bits masked down (valid)  */
    { CAN_OK,                true,  CAN_OK,         true,  false, false }, /* 6 high id bits       */
    { CAN_OK,                true,  CAN_OK,         true,  false, true },  /* 7 all 0xFF payload   */
    { CAN_OK,                true,  CAN_OK,         true,  true,  false }, /* 8 bad checksum       */
    { CAN_OK,                true,  CAN_OK,         true,  false, false }, /* 9 oversized garbage  */
    { CAN_OK,                true,  CAN_OK,         true,  false, false }, /* 10 zero-length frame */
    { CAN_OK,                true,  CAN_OK,         true,  false, false }, /* 11 max ext id        */
};
#define EXPECTED_CORRUPT_COUNT (sizeof(expected_corrupt) / sizeof(expected_corrupt[0]))

/* ----------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */
static int read_entire_file(const char *path, uint8_t **out_buf, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        printf("[FAIL] %s:%d: could not open fixture '%s'\n", __FILE__, __LINE__, path);
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        printf("[FAIL] %s:%d: fseek SEEK_END failed for '%s'\n", __FILE__, __LINE__, path);
        return 1;
    }
    long sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        printf("[FAIL] %s:%d: ftell failed for '%s'\n", __FILE__, __LINE__, path);
        return 1;
    }
    rewind(fp);
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (buf == NULL) {
        fclose(fp);
        printf("[FAIL] %s:%d: out of memory reading '%s'\n", __FILE__, __LINE__, path);
        return 1;
    }
    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (rd != (size_t)sz) {
        free(buf);
        printf("[FAIL] %s:%d: short read on '%s'\n", __FILE__, __LINE__, path);
        return 1;
    }
    *out_buf = buf;
    *out_len = (size_t)sz;
    return 0;
}

static int hex_string_to_bytes(const char *hex, uint8_t *out, size_t out_cap) {
    size_t len = strlen(hex);
    if (len % 2 != 0) return -1;
    size_t nbytes = len / 2;
    if (nbytes > out_cap) return -1;
    for (size_t i = 0; i < nbytes; i++) {
        unsigned int v;
        if (sscanf(hex + 2 * i, "%02x", &v) != 1) return -1;
        out[i] = (uint8_t)v;
    }
    return (int)nbytes;
}

/* ----------------------------------------------------------------------
 * Raw binary fixture validation
 * ---------------------------------------------------------------------- */
int test_raw_frames_fixture(void) {
    printf("\n=== Validating raw_frames.bin fixture ===\n");

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    TEST_ASSERT(read_entire_file(RAW_FRAMES_PATH, &buf, &buf_len) == 0,
                "raw_frames.bin read successfully");
    TEST_ASSERT(buf_len > 0, "raw_frames.bin is non-empty");

    size_t offset = 0;
    for (size_t i = 0; i < EXPECTED_RAW_COUNT; i++) {
        const fixture_expected_frame_t *exp = &expected_raw_frames[i];

        TEST_ASSERT(offset + 5u <= buf_len,
                    "Enough bytes remain for frame header");

        can_frame_t frame;
        can_frame_init(&frame);
        can_error_t rc = can_frame_from_wire(buf + offset, buf_len - offset, &frame);
        TEST_ASSERT(rc == CAN_OK, "can_frame_from_wire returns CAN_OK for valid fixture frame");

        TEST_ASSERT(frame.can_id == exp->can_id, "Frame id matches fixture");
        TEST_ASSERT(frame.can_dlc == exp->dlc, "Frame dlc matches fixture");
        TEST_ASSERT(frame.is_extended == exp->is_extended,
                    "Frame extended flag matches fixture");
        TEST_ASSERT(frame.is_rtr == exp->is_rtr, "Frame rtr flag matches fixture");
        TEST_ASSERT(frame.is_error == exp->is_error, "Frame error flag matches fixture");
        if (exp->dlc > 0) {
            TEST_ASSERT(memcmp(frame.data, exp->data, exp->dlc) == 0,
                        "Frame payload matches fixture");
        }
        TEST_ASSERT(can_frame_is_valid(&frame), "Parsed fixture frame is valid");

        offset += 5u + (size_t)frame.can_dlc;
    }
    TEST_ASSERT(offset == buf_len, "Consumed entire fixture buffer exactly");

    free(buf);
    return 0;
}

/* ----------------------------------------------------------------------
 * Extended binary fixture validation
 * ---------------------------------------------------------------------- */
int test_extended_frames_fixture(void) {
    printf("\n=== Validating extended_frames.bin fixture ===\n");

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    TEST_ASSERT(read_entire_file(EXTENDED_FRAMES_PATH, &buf, &buf_len) == 0,
                "extended_frames.bin read successfully");
    TEST_ASSERT(buf_len > 0, "extended_frames.bin is non-empty");

    size_t offset = 0;
    for (size_t i = 0; i < EXPECTED_EXT_COUNT; i++) {
        const fixture_expected_frame_t *exp = &expected_extended_frames[i];

        TEST_ASSERT(offset + 5u <= buf_len, "Remaining bytes sufficient for extended frame header");

        can_frame_t frame;
        can_frame_init(&frame);
        can_error_t rc = can_frame_from_wire(buf + offset, buf_len - offset, &frame);
        TEST_ASSERT(rc == CAN_OK, "can_frame_from_wire parses extended frame fixture");
        TEST_ASSERT(frame.can_id == exp->can_id, "Extended frame ID matches fixture");
        TEST_ASSERT(frame.can_dlc == exp->dlc, "Extended frame DLC matches fixture");
        TEST_ASSERT(frame.is_extended == true, "Extended frame flag is true");
        if (exp->dlc > 0) {
            TEST_ASSERT(memcmp(frame.data, exp->data, exp->dlc) == 0,
                        "Extended frame payload matches fixture");
        }
        offset += 5u + (size_t)frame.can_dlc;
    }
    TEST_ASSERT(offset == buf_len, "Consumed entire extended fixture buffer exactly");

    free(buf);
    return 0;
}

/* ----------------------------------------------------------------------
 * Corrupted hex fixture validation
 * ---------------------------------------------------------------------- */
int test_corrupted_frames_fixture(void) {
    printf("\n=== Validating corrupted_frames.hex fixture ===\n");

    FILE *fp = fopen(CORRUPTED_HEX_PATH, "r");
    TEST_ASSERT(fp != NULL, "corrupted_frames.hex opened successfully");

    char line[512];
    size_t corrupt_idx = 0;
    size_t parsed_count = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t llen = strlen(line);
        while (llen > 0 && (line[llen - 1] == '\n' || line[llen - 1] == '\r')) {
            line[--llen] = '\0';
        }
        if (llen == 0) continue;
        if (line[0] == '#') continue;

        TEST_ASSERT(corrupt_idx < EXPECTED_CORRUPT_COUNT,
                    "Hex line has a corresponding expected outcome");

        const fixture_expected_corrupt_t *exp = &expected_corrupt[corrupt_idx];

        uint8_t wire[64];
        int nbytes = hex_string_to_bytes(line, wire, sizeof(wire));
        TEST_ASSERT(nbytes > 0, "Hex line decoded to at least one byte");

        can_frame_t frame;
        can_frame_init(&frame);
        can_error_t rc = can_frame_from_wire(wire, (size_t)nbytes, &frame);

        TEST_ASSERT(rc == exp->wire_result,
                    "Corrupt frame returns expected wire error code");
        TEST_ASSERT((rc == CAN_OK) == exp->wire_ok,
                    "Wire parse success flag matches expectation");

        if (exp->wire_ok) {
            parsed_count++;

            if (exp->check_integrity) {
                TEST_ASSERT(can_validate_frame_integrity(&frame) == exp->integrity_result,
                            "can_validate_frame_integrity returns expected code");
            }

            if (exp->all_0xff_payload) {
                bool all_ff = true;
                for (uint8_t i = 0; i < frame.can_dlc; i++) {
                    if (frame.data[i] != 0xFFu) { all_ff = false; break; }
                }
                TEST_ASSERT(all_ff, "Malicious all-0xFF payload preserved intact");
            }

            if (exp->check_checksum) {
                uint8_t computed = can_checksum_compute(frame.data, frame.can_dlc - 1u);
                TEST_ASSERT(frame.data[frame.can_dlc - 1u] != computed,
                            "Fixture trailing byte differs from true CRC-8");
                TEST_ASSERT(!can_checksum_validate(frame.data, frame.can_dlc - 1u,
                                                   frame.data[frame.can_dlc - 1u]),
                            "Checksum validation fails on corrupted payload");
            }

            TEST_ASSERT(frame.can_dlc <= CAN_DLC_MAX, "Parsed dlc within bounds");
        }

        corrupt_idx++;
    }

    fclose(fp);

    TEST_ASSERT(corrupt_idx == EXPECTED_CORRUPT_COUNT,
                "Every expected corrupt case was exercised");
    TEST_ASSERT(parsed_count > 0, "At least one structurally-valid corrupt frame parsed");
    return 0;
}

/* ----------------------------------------------------------------------
 * Bus-off sequence hex fixture validation
 * ---------------------------------------------------------------------- */
int test_bus_off_sequence_fixture(void) {
    printf("\n=== Validating bus_off_sequence.hex fixture ===\n");

    FILE *fp = fopen(BUS_OFF_HEX_PATH, "r");
    TEST_ASSERT(fp != NULL, "bus_off_sequence.hex opened successfully");

    char line[512];
    size_t line_count = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t llen = strlen(line);
        while (llen > 0 && (line[llen - 1] == '\n' || line[llen - 1] == '\r')) {
            line[--llen] = '\0';
        }
        if (llen == 0) continue;
        if (line[0] == '#') continue;

        uint8_t wire[64];
        int nbytes = hex_string_to_bytes(line, wire, sizeof(wire));
        TEST_ASSERT(nbytes > 0, "Bus-off hex entry decodes to bytes");

        can_frame_t frame;
        can_frame_init(&frame);
        can_error_t rc = can_frame_from_wire(wire, (size_t)nbytes, &frame);
        TEST_ASSERT(rc == CAN_OK, "Bus-off sequence line parses successfully");

        line_count++;
    }

    fclose(fp);
    TEST_ASSERT(line_count == 10, "Parsed all 10 bus-off sequence fixture frames");
    return 0;
}

int run_all_fixture_tests(void) {
    int failed = 0;
    failed += test_raw_frames_fixture();
    failed += test_corrupted_frames_fixture();
    failed += test_extended_frames_fixture();
    failed += test_bus_off_sequence_fixture();
    return failed;
}

int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER FIXTURE VALIDATION TEST SUITE \n");
    printf("====================================================\n");

    int failed = run_all_fixture_tests();

    printf("\n====================================================\n");
    if (failed == 0) {
        printf(" ALL FIXTURE VALIDATION TESTS PASSED SUCCESSFULLY! \n");
    } else {
        printf(" FIXTURE VALIDATION FAILURES: %d\n", failed);
    }
    printf("====================================================\n");

    return failed;
}
