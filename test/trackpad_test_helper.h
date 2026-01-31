/**
 * @file trackpad_test_helper.h
 * @brief Test infrastructure for trackpad gesture testing
 *
 * Provides:
 * - Action recorder (spy pattern) to capture output actions
 * - Test context builder for readable test DSL
 * - Helper functions for creating input events
 */

#pragma once

#include "trackpad_gesture.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========================== Action Recorder (Spy Pattern) ==========================

#define MAX_RECORDED_ACTIONS 50

/**
 * @brief Recorded action with metadata
 */
typedef struct {
    trackpad_action_type_t type;
    int16_t dx;
    int16_t dy;
    uint8_t buttons;
    int8_t scroll_v;
    int8_t scroll_h;
    uint32_t timestamp;  // When this action was generated
} recorded_action_t;

/**
 * @brief Action recorder (spy)
 */
typedef struct {
    recorded_action_t actions[MAX_RECORDED_ACTIONS];
    int count;
} action_recorder_t;

/**
 * @brief Reset recorder to empty
 */
void recorder_reset(action_recorder_t *rec);

/**
 * @brief Add action to recorder
 */
void recorder_add(action_recorder_t *rec, const trackpad_action_t *action, uint32_t timestamp);

/**
 * @brief Check if recorder contains action of specific type
 */
bool recorder_has_action(const action_recorder_t *rec, trackpad_action_type_t type);

/**
 * @brief Count actions of specific type
 */
int recorder_count_type(const action_recorder_t *rec, trackpad_action_type_t type);

/**
 * @brief Get first action of specific type (NULL if not found)
 */
const recorded_action_t* recorder_find_first(const action_recorder_t *rec,
                                             trackpad_action_type_t type);

/**
 * @brief Print recorded actions (for debugging)
 */
void recorder_print(const action_recorder_t *rec);

// ========================== Test Context Builder (DSL) ==========================

/**
 * @brief Test context for readable tests
 */
typedef struct {
    trackpad_state_t *state;
    action_recorder_t *recorder;
    uint32_t current_time;  // Simulated time
} test_context_t;

/**
 * @brief Begin a test - creates context
 *
 * @param hres Horizontal resolution
 * @param vres Vertical resolution
 * @param scroll_w Scroll zone width
 * @param scroll_h Scroll zone height
 * @return Test context (must be freed with test_end)
 */
test_context_t* test_begin(uint16_t hres, uint16_t vres,
                           int32_t scroll_w, int32_t scroll_h);

/**
 * @brief End test - frees context
 */
void test_end(test_context_t *ctx);

/**
 * @brief Simulate touch down at position
 */
void test_touch_down(test_context_t *ctx, int32_t x, int32_t y);

/**
 * @brief Simulate touch move to position
 */
void test_touch_move(test_context_t *ctx, int32_t x, int32_t y);

/**
 * @brief Simulate touch up at position
 */
void test_touch_up(test_context_t *ctx, int32_t x, int32_t y);

/**
 * @brief Advance time by milliseconds
 */
void test_advance_time(test_context_t *ctx, uint32_t ms);

/**
 * @brief Simulate complete tap gesture
 */
void test_tap_at(test_context_t *ctx, int32_t x, int32_t y, uint32_t duration_ms);

/**
 * @brief Simulate swipe gesture
 */
void test_swipe(test_context_t *ctx, int32_t x1, int32_t y1,
                int32_t x2, int32_t y2, uint32_t duration_ms);

/**
 * @brief Simulate drag gesture (tap-tap-drag)
 */
void test_drag(test_context_t *ctx, int32_t x1, int32_t y1,
               int32_t x2, int32_t y2);

// ========================== Input Event Helpers ==========================

/**
 * @brief Create pressed event
 */
trackpad_input_t make_pressed_event(int32_t x, int32_t y, uint32_t time);

/**
 * @brief Create pressing event
 */
trackpad_input_t make_pressing_event(int32_t x, int32_t y, uint32_t time);

/**
 * @brief Create released event
 */
trackpad_input_t make_released_event(int32_t x, int32_t y, uint32_t time);

#ifdef __cplusplus
}
#endif
