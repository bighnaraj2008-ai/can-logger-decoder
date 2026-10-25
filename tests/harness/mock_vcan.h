#ifndef HARNESS_MOCK_VCAN_H
#define HARNESS_MOCK_VCAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "can_core.h"

void mock_vcan_init(void);
void mock_vcan_close(void);
void mock_vcan_set_drop_rate(uint8_t percent);
int mock_vcan_send(const can_frame_t *frame);
int mock_vcan_recv(can_frame_t *frame);
size_t mock_vcan_generate_burst(uint32_t base_id, size_t count);
size_t mock_vcan_available(void);

#endif /* HARNESS_MOCK_VCAN_H */
