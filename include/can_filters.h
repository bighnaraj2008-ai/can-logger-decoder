#ifndef CAN_FILTERS_H
#define CAN_FILTERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "can_core.h"

/**
 * @brief Maximum number of acceptance filters kept in a filter set.
 *        (MCP2515 hardware provides 6 filters across 2 receive buffers;
 *         this limit keeps the software model small and fixed-size.)
 */
#define CAN_FILTER_COUNT_MAX 4u

typedef enum {
    CAN_FILTER_MODE_STANDARD = 0,  /**< Accept 11-bit identifiers only */
    CAN_FILTER_MODE_EXTENDED = 1,  /**< Accept 29-bit identifiers only */
    CAN_FILTER_MODE_DUAL     = 2   /**< Accept both standard and extended */
} can_filter_mode_t;

/**
 * @brief Single acceptance filter entry.
 *
 * A frame matches when (clean_id & mask) == (id & mask) and the format
 * (standard/extended) is compatible with mode.
 */
typedef struct {
    uint32_t           id;       /**< Filter identifier */
    uint32_t           mask;     /**< Mask bits (1 = compare, 0 = don't care) */
    can_filter_mode_t  mode;     /**< Standard / extended / dual */
    bool               enabled;  /**< Whether this entry is active */
} can_filter_t;

/**
 * @brief Acceptance filter set with optional promiscuous mode.
 */
typedef struct {
    can_filter_t filters[CAN_FILTER_COUNT_MAX];
    size_t       count;
    bool         promiscuous;   /**< When true, every frame is accepted */
} can_filter_set_t;

/**
 * @brief Initialize a filter set to the empty, non-promiscuous state.
 */
void can_filter_set_init(can_filter_set_t *set);

/**
 * @brief Append an acceptance filter entry.
 */
void can_filter_set_add(can_filter_set_t *set,
                        uint32_t id, uint32_t mask, can_filter_mode_t mode);

/**
 * @brief Remove all entries and disable promiscuous mode.
 */
void can_filter_set_clear(can_filter_set_t *set);

/**
 * @brief Test whether a frame is accepted by the filter set.
 *
 * @return true if the frame should be accepted, false otherwise
 */
bool can_filter_match(const can_filter_set_t *set, const can_frame_t *frame);

/**
 * @brief Return the index of the first matching filter, or -1 when none match.
 *        Returns 0 when promiscuous mode is enabled.
 */
int can_filter_route(const can_filter_set_t *set, const can_frame_t *frame);

#endif /* CAN_FILTERS_H */
