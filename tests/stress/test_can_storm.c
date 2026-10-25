#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"

#define STORM_FRAME_COUNT 50000

int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER HIGH-FREQUENCY CAN STORM TEST \n");
    printf("====================================================\n");

    uint32_t processed = 0;
    uint32_t failed = 0;

    for (uint32_t i = 0; i < STORM_FRAME_COUNT; i++) {
        can_frame_t frame;
        can_frame_init(&frame);

        /* Alternate between standard and extended frames with varying DLCs */
        uint8_t dlc = (uint8_t)(i % 9); /* 0 to 8 */
        bool is_ext = (i % 2 == 1);
        uint32_t id = is_ext ? (CAN_EFF_FLAG | (0x18000000u + (i % 0xFFFFu)))
                             : (0x100u + (i % 0x600u));

        frame.can_id = id;
        frame.can_dlc = dlc;
        frame.is_extended = is_ext;

        for (uint8_t b = 0; b < dlc; b++) {
            frame.data[b] = (uint8_t)((i + b * 17) & 0xFF);
        }

        /* Serialize to wire format */
        uint8_t wire[64];
        size_t wire_len = sizeof(wire);
        can_error_t rc_to = can_frame_to_wire(&frame, wire, &wire_len);
        if (rc_to != CAN_OK) {
            failed++;
            continue;
        }

        /* Deserialize back from wire format */
        can_frame_t recovered;
        can_frame_init(&recovered);
        can_error_t rc_from = can_frame_from_wire(wire, wire_len, &recovered);
        if (rc_from != CAN_OK) {
            failed++;
            continue;
        }

        /* Assert fidelity */
        if (recovered.can_id != frame.can_id ||
            recovered.can_dlc != frame.can_dlc ||
            recovered.is_extended != frame.is_extended) {
            failed++;
            continue;
        }

        if (dlc > 0 && memcmp(recovered.data, frame.data, dlc) != 0) {
            failed++;
            continue;
        }

        processed++;
    }

    printf(" Storm Frames Processed: %u / %u\n", processed, STORM_FRAME_COUNT);
    printf(" Validation Failures:     %u\n", failed);
    printf("====================================================\n");

    if (failed == 0 && processed == STORM_FRAME_COUNT) {
        printf(" [PASS] CAN frame storm stress test completed with 100%% fidelity!\n");
        return 0;
    } else {
        printf(" [FAIL] Encountered %u failures during CAN storm stress test.\n", failed);
        return 1;
    }
}
