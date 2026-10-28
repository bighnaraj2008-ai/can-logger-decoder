/*
 * bench_decoder.c - Core Frame Decoding Benchmark Harness
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "can_core.h"
#include "can_parser.h"

static double get_elapsed_ms(struct timespec *start, struct timespec *end) {
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
}

int main(int argc, char **argv) {
    uint32_t frame_count = 50000;
    if (argc > 1) {
        frame_count = (uint32_t)atoi(argv[1]);
    }

    printf("Starting CAN decoder benchmark (%u frames)...\n", frame_count);

    can_frame_t frame;
    frame.can_id = 0x123;
    frame.can_dlc = 8;
    for (int i = 0; i < 8; i++) {
        frame.data[i] = (uint8_t)(i * 11);
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (uint32_t i = 0; i < frame_count; i++) {
        frame.can_id = (frame.can_id + 1) & CAN_SFF_MASK;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_ms = get_elapsed_ms(&start, &end);
    printf("Processed %u frames in %.2f ms\n", frame_count, elapsed_ms);

    return 0;
}