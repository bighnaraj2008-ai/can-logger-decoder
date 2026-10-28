/*
 * bench_decoder.c - CAN Decoder Throughput & Latency Benchmark Harness
 *
 * Rigorous frame throughput and latency benchmarking for can-logger-decoder.
 * Measures frames-per-second (FPS) processing capacity under heavy bus loads
 * using the mock CAN bus generator, plus per-frame latency statistics
 * (min/avg/max microseconds).
 *
 * Build via CMake: cmake -S . -B build && cmake --build build --target bench_decoder
 *
 * Usage:
 *   ./bench_decoder [--frames N] [--rate HZ] [--dbc sample.dbc] [--quiet]
 *
 * Options:
 *   --frames N     Number of frames to process (default: 100000)
 *   --rate HZ      Mock generator frequency in Hz (default: 10000)
 *   --dbc PATH     Path to Vector DBC database (optional; enables signal decode)
 *   --quiet        Print summary only
 *   --help         Show this help message
 */

#undef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "../include/can_parser.h"
#include "../include/dbc_decoder.h"

#ifndef INFINITY
#define INFINITY 1e300
#endif

/* ------------------------------------------------------------------ */
/* High-resolution monotonic timer helpers                            */
/* ------------------------------------------------------------------ */

static double now_us(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return -1.0;
    }
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

/* ------------------------------------------------------------------ */
/* Latency statistics accumulator                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    double min_us;
    double max_us;
    double sum_us;
    double sum_sq_us;
    unsigned long long count;
} latency_stats_t;

static void latency_init(latency_stats_t *s) {
    s->min_us = INFINITY;
    s->max_us = 0.0;
    s->sum_us = 0.0;
    s->sum_sq_us = 0.0;
    s->count = 0;
}

static void latency_add(latency_stats_t *s, double us) {
    if (us < s->min_us) s->min_us = us;
    if (us > s->max_us) s->max_us = us;
    s->sum_us += us;
    s->sum_sq_us += us * us;
    s->count++;
}

static double latency_avg(const latency_stats_t *s) {
    return s->count > 0 ? s->sum_us / (double)s->count : 0.0;
}

static double latency_stddev(const latency_stats_t *s) {
    if (s->count == 0) return 0.0;
    double mean = latency_avg(s);
    double var = s->sum_sq_us / (double)s->count - mean * mean;
    /* Simple integer square root approximation for stddev */
    if (var <= 0.0) return 0.0;
    double guess = var / 2.0;
    for (int i = 0; i < 10; i++) {
        guess = (guess + var / guess) / 2.0;
    }
    return guess;
}

/* ------------------------------------------------------------------ */
/* Argument parsing                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned long long frame_count;
    int      mock_rate_hz;
    const char *dbc_path;
    bool     quiet;
} bench_config_t;

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("CAN Decoder Throughput & Latency Benchmark\n");
    printf("Options:\n");
    printf("  --frames N      Number of frames to process (default: 100000)\n");
    printf("  --rate HZ       Mock generator frequency in Hz (default: 10000)\n");
    printf("  --dbc PATH      Path to Vector DBC database (optional)\n");
    printf("  --quiet         Print summary only\n");
    printf("  --help          Show this help message\n");
}

static int parse_u64(const char *str, unsigned long long *out) {
    char *end = NULL;
    unsigned long long v = strtoull(str, &end, 10);
    if (end == str || *end != '\0' || v == 0) return -1;
    *out = (unsigned long long)v;
    return 0;
}

static int parse_int(const char *str, int *out) {
    char *end = NULL;
    long v = strtol(str, &end, 10);
    if (end == str || *end != '\0' || v <= 0 || v > 1000000) return -1;
    *out = (int)v;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Main benchmark                                                     */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    bench_config_t cfg = {
        .frame_count = 100000,
        .mock_rate_hz = 10000,
        .dbc_path = NULL,
        .quiet = false,
    };

    /* --- Parse arguments --- */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &cfg.frame_count) != 0) {
                fprintf(stderr, "Error: invalid --frames value '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            if (parse_int(argv[++i], &cfg.mock_rate_hz) != 0) {
                fprintf(stderr, "Error: invalid --rate value '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--dbc") == 0 && i + 1 < argc) {
            cfg.dbc_path = argv[++i];
        } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            cfg.quiet = true;
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!cfg.quiet) {
        printf("================================================================\n");
        printf("  CAN DECODER THROUGHPUT & LATENCY BENCHMARK\n");
        printf("================================================================\n");
        printf("  Target frames     : %llu\n", (unsigned long long)cfg.frame_count);
        printf("  Mock rate         : %d Hz\n", cfg.mock_rate_hz);
        printf("  DBC database      : %s\n", cfg.dbc_path ? cfg.dbc_path : "(none)");
        printf("----------------------------------------------------------------\n");
    }

    /* --- Load DBC database (optional) --- */
    dbc_database_t *dbc = NULL;
    if (cfg.dbc_path) {
        dbc = dbc_load_file(cfg.dbc_path);
        if (!dbc) {
            fprintf(stderr, "Warning: could not load DBC '%s' â€” running raw-frame benchmark\n",
                    cfg.dbc_path);
        } else if (!cfg.quiet) {
            printf("  DBC loaded        : %zu messages defined\n", dbc->message_count);
        }
    }

    /* --- Initialize CAN bus (mock mode) --- */
    can_bus_t *bus = can_bus_init("vcan0", true, cfg.mock_rate_hz);
    if (!bus) {
        fprintf(stderr, "Error: failed to initialize CAN bus (mock mode)\n");
        if (dbc) dbc_free(dbc);
        return 1;
    }
    if (!cfg.quiet) {
        printf("  Bus interface     : %s (mock)\n", can_bus_get_interface(bus));
    }

    /* --- Run benchmark --- */
    latency_stats_t stats;
    latency_init(&stats);

    can_msg_t msg;
    unsigned long long frames_processed = 0;
    unsigned long long frames_decoded = 0;
    double t_start = now_us();

    while (frames_processed < cfg.frame_count) {
        double t_frame_start = now_us();

        int res = can_bus_read(bus, &msg, 100);
        if (res <= 0) {
            /* Timeout or transient error â€” skip */
            continue;
        }

        double t_frame_end = now_us();
        double latency_us = t_frame_end - t_frame_start;
        latency_add(&stats, latency_us);

        frames_processed++;

        /* Decode if DBC available */
        if (dbc) {
            const dbc_message_t *msg_def = dbc_find_message(dbc, msg.can_id);
            if (msg_def) {
                dbc_decoded_message_t decoded;
                int n = dbc_decode_message(msg_def, msg.data, msg.dlc, &decoded);
                if (n > 0) frames_decoded++;
            }
        }

        if (!cfg.quiet && (frames_processed % 10000 == 0)) {
            double elapsed_s = (now_us() - t_start) / 1e6;
            double fps = elapsed_s > 0.0 ? (double)frames_processed / elapsed_s : 0.0;
            printf("  Processed %llu frames ... %.1f FPS\n",
                   (unsigned long long)frames_processed, fps);
        }
    }

    double t_end = now_us();
    double elapsed_s = (t_end - t_start) / 1e6;
    double fps = elapsed_s > 0.0 ? (double)frames_processed / elapsed_s : 0.0;
    double avg_latency = latency_avg(&stats);
    double stddev_latency = latency_stddev(&stats);

    /* --- Report results --- */
    printf("================================================================\n");
    printf("  BENCHMARK RESULTS\n");
    printf("================================================================\n");
    printf("  Frames processed  : %llu\n", (unsigned long long)frames_processed);
    printf("  Elapsed time      : %.6f seconds\n", elapsed_s);
    printf("  Throughput        : %.2f frames/sec\n", fps);
    printf("  Latency (min)     : %.3f Âµs\n", stats.min_us);
    printf("  Latency (avg)     : %.3f Âµs (Â±%.3f Âµs stddev)\n", avg_latency, stddev_latency);
    printf("  Latency (max)     : %.3f Âµs\n", stats.max_us);
    if (dbc) {
        printf("  Signals decoded   : %llu\n", (unsigned long long)frames_decoded);
    }
    printf("================================================================\n");

    /* --- Teardown --- */
    can_bus_close(bus);
    if (dbc) dbc_free(dbc);

    return 0;
}
