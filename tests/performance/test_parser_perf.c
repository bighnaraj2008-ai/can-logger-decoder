#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "can_core.h"
#include "error_codes.h"

#define BENCHMARK_ITERATIONS 100000

int main(void) {
    printf("====================================================\n");
    printf(" CAN LOGGER & DECODER PARSER PERFORMANCE BENCHMARK \n");
    printf("====================================================\n");

    /* Prepare a sample wire frame: 4 bytes ID (0x100) + 1 byte DLC (8) + 8 data bytes */
    uint8_t wire_frame[13] = {
        0x00, 0x00, 0x01, 0x00, 0x08,
        0x10, 0x27, 0x72, 0x1F, 0x7F, 0x82, 0xAA, 0x55
    };

    can_frame_t parsed_frame;
    clock_t start = clock();

    int parse_errors = 0;
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        can_frame_init(&parsed_frame);
        can_error_t rc = can_frame_from_wire(wire_frame, sizeof(wire_frame), &parsed_frame);
        if (rc != CAN_OK) {
            parse_errors++;
        }
    }

    clock_t end = clock();
    double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
    if (elapsed_sec <= 0.0) elapsed_sec = 0.0001; /* avoid div zero */

    double fps = (double)BENCHMARK_ITERATIONS / elapsed_sec;
    double latency_ns = (elapsed_sec * 1e9) / (double)BENCHMARK_ITERATIONS;

    printf(" Iterations:     %d\n", BENCHMARK_ITERATIONS);
    printf(" Elapsed Time:   %.4f seconds\n", elapsed_sec);
    printf(" Throughput:     %.2f frames/sec\n", fps);
    printf(" Mean Latency:   %.1f ns/frame\n", latency_ns);
    printf(" Parse Failures: %d\n", parse_errors);

    printf("====================================================\n");
    if (parse_errors == 0 && fps > 100000.0) {
        printf(" [PASS] Parser meets high-performance throughput spec!\n");
        return 0;
    } else if (parse_errors == 0) {
        printf(" [PASS] Parser benchmarks completed without errors.\n");
        return 0;
    } else {
        printf(" [FAIL] Encountered parse errors during performance benchmark.\n");
        return 1;
    }
}
