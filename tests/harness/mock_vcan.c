/*
 * Linux SocketCAN Virtual Network Interface Mock Harness
 * Emulates non-blocking ring buffer queues and synthetic automotive packet streams.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"

#define MOCK_VCAN_QUEUE_CAPACITY 128

typedef struct {
    can_frame_t queue[MOCK_VCAN_QUEUE_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t packets_dropped;
    uint8_t drop_rate_percent;
    bool is_open;
} mock_vcan_socket_t;

static mock_vcan_socket_t g_mock_vcan;

void mock_vcan_init(void) {
    memset(&g_mock_vcan, 0, sizeof(g_mock_vcan));
    g_mock_vcan.is_open = true;
}

void mock_vcan_close(void) {
    g_mock_vcan.is_open = false;
    g_mock_vcan.count = 0;
    g_mock_vcan.head = 0;
    g_mock_vcan.tail = 0;
}

void mock_vcan_set_drop_rate(uint8_t percent) {
    g_mock_vcan.drop_rate_percent = (percent > 100) ? 100 : percent;
}

int mock_vcan_send(const can_frame_t *frame) {
    if (!g_mock_vcan.is_open || frame == NULL) {
        return -1;
    }
    if (g_mock_vcan.drop_rate_percent > 0 &&
        (rand() % 100) < g_mock_vcan.drop_rate_percent) {
        g_mock_vcan.packets_dropped++;
        return 0; /* dropped */
    }
    if (g_mock_vcan.count >= MOCK_VCAN_QUEUE_CAPACITY) {
        g_mock_vcan.packets_dropped++;
        return -1; /* overflow */
    }

    g_mock_vcan.queue[g_mock_vcan.tail] = *frame;
    g_mock_vcan.tail = (g_mock_vcan.tail + 1) % MOCK_VCAN_QUEUE_CAPACITY;
    g_mock_vcan.count++;
    g_mock_vcan.packets_sent++;
    return 0;
}

int mock_vcan_recv(can_frame_t *frame) {
    if (!g_mock_vcan.is_open || frame == NULL || g_mock_vcan.count == 0) {
        return -1;
    }

    *frame = g_mock_vcan.queue[g_mock_vcan.head];
    g_mock_vcan.head = (g_mock_vcan.head + 1) % MOCK_VCAN_QUEUE_CAPACITY;
    g_mock_vcan.count--;
    g_mock_vcan.packets_received++;
    return 0;
}

size_t mock_vcan_generate_burst(uint32_t base_id, size_t count) {
    size_t generated = 0;
    for (size_t i = 0; i < count; i++) {
        can_frame_t frame;
        can_frame_init(&frame);
        frame.can_id = base_id + (uint32_t)(i % 16);
        frame.can_dlc = 8;
        for (uint8_t d = 0; d < 8; d++) {
            frame.data[d] = (uint8_t)((i + d) & 0xFF);
        }
        if (mock_vcan_send(&frame) == 0) {
            generated++;
        }
    }
    return generated;
}

size_t mock_vcan_available(void) {
    return g_mock_vcan.count;
}
