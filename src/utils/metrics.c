#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"

typedef struct {
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t error_frames;
    uint64_t dropped_frames;
    uint64_t bus_off_events;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    double   start_time_sec;
    double   last_update_sec;
} can_metrics_t;

static double get_now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
}

void can_metrics_init(can_metrics_t *m) {
    if (m == NULL) return;
    memset(m, 0, sizeof(*m));
    m->start_time_sec = get_now_sec();
    m->last_update_sec = m->start_time_sec;
}

void can_metrics_reset(can_metrics_t *m) {
    can_metrics_init(m);
}

void can_metrics_record_rx(can_metrics_t *m, const can_frame_t *frame) {
    if (m == NULL) return;
    m->rx_frames++;
    if (frame != NULL) {
        m->rx_bytes += frame->can_dlc;
        if (frame->is_error) {
            m->error_frames++;
        }
    }
    m->last_update_sec = get_now_sec();
}

void can_metrics_record_tx(can_metrics_t *m, const can_frame_t *frame) {
    if (m == NULL) return;
    m->tx_frames++;
    if (frame != NULL) {
        m->tx_bytes += frame->can_dlc;
    }
    m->last_update_sec = get_now_sec();
}

void can_metrics_record_drop(can_metrics_t *m) {
    if (m == NULL) return;
    m->dropped_frames++;
    m->last_update_sec = get_now_sec();
}

void can_metrics_record_bus_off(can_metrics_t *m) {
    if (m == NULL) return;
    m->bus_off_events++;
    m->last_update_sec = get_now_sec();
}

double can_metrics_get_rx_fps(const can_metrics_t *m) {
    if (m == NULL) return 0.0;
    double elapsed = m->last_update_sec - m->start_time_sec;
    if (elapsed <= 0.0) return 0.0;
    return (double)m->rx_frames / elapsed;
}

double can_metrics_get_rx_kbps(const can_metrics_t *m) {
    if (m == NULL) return 0.0;
    double elapsed = m->last_update_sec - m->start_time_sec;
    if (elapsed <= 0.0) return 0.0;
    /* (bytes * 8) / (1000 * elapsed) */
    return ((double)m->rx_bytes * 8.0) / (1000.0 * elapsed);
}

void can_metrics_summary(const can_metrics_t *m, char *buf, size_t len) {
    if (m == NULL || buf == NULL || len == 0) return;
    snprintf(buf, len, "RX: %llu, TX: %llu, Drops: %llu, Errors: %llu, Bus-Offs: %llu",
             (unsigned long long)m->rx_frames,
             (unsigned long long)m->tx_frames,
             (unsigned long long)m->dropped_frames,
             (unsigned long long)m->error_frames,
             (unsigned long long)m->bus_off_events);
}
