#ifndef TEST_FIXTURES_H
#define TEST_FIXTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "can_core.h"
#include "error_codes.h"

#define FIXTURE_DIR "tests/fixtures"
#define RAW_FRAMES_PATH       FIXTURE_DIR "/raw_frames.bin"
#define CORRUPTED_HEX_PATH    FIXTURE_DIR "/corrupted_frames.hex"
#define EXTENDED_FRAMES_PATH  FIXTURE_DIR "/extended_frames.bin"
#define BUS_OFF_HEX_PATH      FIXTURE_DIR "/bus_off_sequence.hex"

/* ----------------------------------------------------------------------
 * Wire layout descriptor for expected frames
 * Wire layout: [id_b3][id_b2][id_b1][id_b0][dlc][data...]
 * ---------------------------------------------------------------------- */
typedef struct {
    uint32_t  can_id;        /* raw id as it appears on the wire (flags included) */
    uint8_t   dlc;
    bool      is_extended;
    bool      is_rtr;
    bool      is_error;
    uint8_t   data[CAN_FRAME_MAX_DATA_LEN];
} fixture_expected_frame_t;

/* ----------------------------------------------------------------------
 * Expected corrupted-frame outcomes
 * ---------------------------------------------------------------------- */
typedef struct {
    can_error_t wire_result;        /* expected can_frame_from_wire result */
    bool        wire_ok;            /* true if wire parse should succeed */
    can_error_t integrity_result;   /* expected can_validate_frame_integrity result */
    bool        check_integrity;    /* whether to run integrity validation */
    bool        check_checksum;     /* whether to run checksum validation */
    bool        all_0xff_payload;   /* resilience check: all payload bytes 0xFF */
} fixture_expected_corrupt_t;

/* Function prototypes for fixture validation */
int test_raw_frames_fixture(void);
int test_corrupted_frames_fixture(void);
int test_extended_frames_fixture(void);
int test_bus_off_sequence_fixture(void);
int run_all_fixture_tests(void);

#endif /* TEST_FIXTURES_H */
