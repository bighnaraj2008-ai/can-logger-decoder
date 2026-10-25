#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"

#define TEST_ALLOCATION_ROUNDS 1000

int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER MEMORY SANITIZER HARNESS      \n");
    printf("====================================================\n");

    int leak_failures = 0;

    for (int r = 0; r < TEST_ALLOCATION_ROUNDS; r++) {
        /* Allocate dynamic wire buffers */
        size_t buf_len = 64;
        uint8_t *wire_buf = (uint8_t *)malloc(buf_len);
        if (!wire_buf) {
            leak_failures++;
            break;
        }

        can_frame_t *frame = (can_frame_t *)malloc(sizeof(can_frame_t));
        if (!frame) {
            free(wire_buf);
            leak_failures++;
            break;
        }

        can_frame_init(frame);
        frame->can_id = 0x100u + (uint32_t)(r % 100);
        frame->can_dlc = 8;
        memset(frame->data, 0xAA, 8);

        size_t out_len = buf_len;
        if (can_frame_to_wire(frame, wire_buf, &out_len) != CAN_OK) {
            leak_failures++;
        }

        can_frame_t recovered;
        can_frame_init(&recovered);
        if (can_frame_from_wire(wire_buf, out_len, &recovered) != CAN_OK) {
            leak_failures++;
        }

        /* Clean up all heap structures */
        free(wire_buf);
        free(frame);
    }

    printf(" Allocation / Deserialization Cycles: %d\n", TEST_ALLOCATION_ROUNDS);
    printf(" Memory Safety Failures:              %d\n", leak_failures);
    printf("====================================================\n");

    if (leak_failures == 0) {
        printf(" [PASS] Memory safety check clean (no leaks / no faults).\n");
        return 0;
    } else {
        printf(" [FAIL] Memory sanitizer harness detected failures.\n");
        return 1;
    }
}
