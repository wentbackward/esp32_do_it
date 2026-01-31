/**
 * @file trackpad_gesture.h
 * @brief Pure gesture processing logic for trackpad (framework-independent)
 *
 * This module contains all the gesture recognition logic extracted from
 * ui_trackpad.c to make it testable without LVGL or hardware dependencies.
 *
 * Key design principles:
 * - Pure functions where possible (acceleration, jitter filtering, zone detection)
 * - Explicit state management (trackpad_state_t) instead of static globals
 * - Time is injected (not queried with lv_tick_get)
 * - Actions are returned (not executed) - caller decides what to do
 * - No LVGL, FreeRTOS, or ESP-IDF dependencies (host-compilable)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========================== Configuration Constants ==========================

// Jitter filtering
#define TRACKPAD_JITTER_THRESHOLD 3

// Velocity smoothing (EWMA)
#define TRACKPAD_VELOCITY_ALPHA 0.3f

// Acceleration curve (velocity in pixels per second)
#define TRACKPAD_ACCEL_PRECISION_SENSITIVITY 0.5f
#define TRACKPAD_ACCEL_BASE_SENSITIVITY 1.0f
#define TRACKPAD_ACCEL_MAX_MULTIPLIER 5.0f
#define TRACKPAD_ACCEL_PRECISION_THRESHOLD 100.0f
#define TRACKPAD_ACCEL_LINEAR_THRESHOLD 400.0f
#define TRACKPAD_ACCEL_MAX_THRESHOLD 1500.0f

// Tap detection
#define TRACKPAD_TAP_MIN_DURATION_MS 50
#define TRACKPAD_TAP_MAX_DURATION_MS 200
#define TRACKPAD_TAP_MOVE_THRESHOLD 15
#define TRACKPAD_TAP_JITTER_RATIO 3.0f

// Tap-tap-drag timing
#define TRACKPAD_DOUBLE_TAP_WINDOW_MS 350
#define TRACKPAD_DRAG_MOVE_THRESHOLD 25

// Scroll sensitivity
#define TRACKPAD_SCROLL_SENSITIVITY 20

// ========================== Types ==========================

/**
 * @brief Touch zone classification
 */
typedef enum {
    TRACKPAD_ZONE_MAIN = 0,      // Main trackpad area (movement)
    TRACKPAD_ZONE_SCROLL_V,      // Right edge (vertical scroll)
    TRACKPAD_ZONE_SCROLL_H,      // Bottom edge (horizontal scroll)
    TRACKPAD_ZONE_SCROLL_CORNER, // Bottom-right corner
} trackpad_zone_t;

/**
 * @brief Touch state machine states
 */
typedef enum {
    TRACKPAD_STATE_IDLE = 0,
    TRACKPAD_STATE_DOWN,
    TRACKPAD_STATE_MOVING,
    TRACKPAD_STATE_SCROLLING,
    TRACKPAD_STATE_DRAGGING,
} trackpad_touch_state_t;

/**
 * @brief Input event types (abstracted from LVGL)
 */
typedef enum {
    TRACKPAD_EVENT_PRESSED,
    TRACKPAD_EVENT_PRESSING,
    TRACKPAD_EVENT_RELEASED,
} trackpad_event_type_t;

/**
 * @brief Input event (framework-independent)
 */
typedef struct {
    trackpad_event_type_t type;
    int32_t x;
    int32_t y;
    uint32_t timestamp_ms;
} trackpad_input_t;

/**
 * @brief Output action types
 */
typedef enum {
    TRACKPAD_ACTION_NONE = 0,
    TRACKPAD_ACTION_MOVE,           // Cursor movement
    TRACKPAD_ACTION_CLICK_DOWN,     // Button press
    TRACKPAD_ACTION_CLICK_UP,       // Button release
    TRACKPAD_ACTION_DOUBLE_CLICK,   // Double-click sequence
    TRACKPAD_ACTION_SCROLL_V,       // Vertical scroll
    TRACKPAD_ACTION_SCROLL_H,       // Horizontal scroll
    TRACKPAD_ACTION_DRAG_START,     // Begin drag (button down)
    TRACKPAD_ACTION_DRAG_MOVE,      // Move while dragging
    TRACKPAD_ACTION_DRAG_END,       // End drag (button up)
    TRACKPAD_ACTION_SHOW_DRAG_INDICATOR,  // UI feedback: show green circle
    TRACKPAD_ACTION_HIDE_DRAG_INDICATOR,  // UI feedback: hide green circle
} trackpad_action_type_t;

/**
 * @brief Output action
 */
typedef struct {
    trackpad_action_type_t type;
    int16_t dx;          // Movement delta (accelerated)
    int16_t dy;
    int8_t scroll_v;     // Scroll units (vertical)
    int8_t scroll_h;     // Scroll units (horizontal)
    uint8_t buttons;     // Button state (0x01=left, 0x02=right, 0x04=middle)
} trackpad_action_t;

/**
 * @brief Point structure (framework-independent)
 */
typedef struct {
    int32_t x;
    int32_t y;
} trackpad_point_t;

/**
 * @brief Trackpad gesture processor state
 *
 * All mutable state for gesture recognition.
 * In production, a single static instance is used.
 * In tests, fresh instances can be created per test case.
 */
typedef struct {
    // Configuration (immutable after init)
    uint16_t hres;
    uint16_t vres;
    int32_t scroll_zone_w;
    int32_t scroll_zone_h;

    // Touch state machine
    trackpad_touch_state_t state;
    trackpad_point_t touch_start;
    trackpad_point_t last_pos;
    uint32_t touch_down_time;
    uint32_t last_sample_time;
    uint32_t last_tap_time;
    int32_t total_movement;
    bool button_held;
    bool tap_tap_pending;

    // Velocity smoothing (EWMA)
    float velocity_x_smooth;
    float velocity_y_smooth;

    // Scroll accumulators
    float scroll_accum_v;
    float scroll_accum_h;
} trackpad_state_t;

/**
 * @brief Tap classification result
 */
typedef enum {
    TRACKPAD_TAP_NONE = 0,      // Not a tap
    TRACKPAD_TAP_SINGLE,        // Single tap (click)
    TRACKPAD_TAP_DOUBLE,        // Double tap (double-click)
} trackpad_tap_result_t;

// ========================== Public API ==========================

/**
 * @brief Initialize trackpad state
 *
 * @param state State structure to initialize
 * @param hres Horizontal resolution (pixels)
 * @param vres Vertical resolution (pixels)
 * @param scroll_zone_w Scroll zone width (pixels)
 * @param scroll_zone_h Scroll zone height (pixels)
 */
void trackpad_state_init(trackpad_state_t *state,
                         uint16_t hres, uint16_t vres,
                         int32_t scroll_zone_w, int32_t scroll_zone_h);

/**
 * @brief Reset trackpad state to idle
 *
 * @param state State to reset
 */
void trackpad_state_reset(trackpad_state_t *state);

/**
 * @brief Process a touch input event
 *
 * Core gesture processor: given current state and input, produces action(s).
 * May modify state. Caller is responsible for executing returned actions.
 *
 * @param state Current gesture processor state (will be modified)
 * @param input Touch input event
 * @param action Output action to execute (only valid if returns true)
 * @return true if action should be executed, false if no action needed
 */
bool trackpad_process_input(trackpad_state_t *state,
                            const trackpad_input_t *input,
                            trackpad_action_t *action);

// ========================== Pure Functions (Testable) ==========================

/**
 * @brief Clamp value between min and max
 */
int32_t trackpad_clamp_i32(int32_t val, int32_t min, int32_t max);

/**
 * @brief Determine which zone a point is in
 *
 * Pure function - no state access.
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @param hres Horizontal resolution
 * @param vres Vertical resolution
 * @param scroll_w Scroll zone width
 * @param scroll_h Scroll zone height
 * @return Zone classification
 */
trackpad_zone_t trackpad_get_zone(int32_t x, int32_t y,
                                  uint16_t hres, uint16_t vres,
                                  int32_t scroll_w, int32_t scroll_h);

/**
 * @brief Apply jitter filtering to movement delta
 *
 * Movements within the dead zone are filtered to zero.
 * Movements outside have the dead zone subtracted.
 *
 * @param raw_delta Raw movement in pixels
 * @param threshold Dead zone threshold
 * @return Filtered delta (0 if within dead zone)
 */
int32_t trackpad_filter_jitter(int32_t raw_delta, int32_t threshold);

/**
 * @brief Check if movement is within jitter threshold
 *
 * @param dx X delta
 * @param dy Y delta
 * @param threshold Jitter threshold
 * @return true if both deltas are within threshold
 */
bool trackpad_is_jitter(int32_t dx, int32_t dy, int32_t threshold);

/**
 * @brief Update EWMA smoothed velocity
 *
 * @param current_smooth Current smoothed value
 * @param instant_value Instantaneous measurement
 * @param alpha Smoothing factor (0.0=very smooth, 1.0=no smoothing)
 * @return New smoothed value
 */
float trackpad_ewma_update(float current_smooth, float instant_value, float alpha);

/**
 * @brief Apply dual-phase acceleration curve
 *
 * Industry-standard approach:
 * - Slow movements: sub-unity multiplier for precise control
 * - Medium movements: linear transition zone
 * - Fast movements: power curve acceleration for quick traversal
 *
 * @param delta Movement delta (pixels)
 * @param velocity_pps Smoothed velocity in pixels per second
 * @return Accelerated movement delta
 */
float trackpad_apply_acceleration(float delta, float velocity_pps);

/**
 * @brief Classify if touch release qualifies as a tap
 *
 * @param duration_ms Touch duration in milliseconds
 * @param net_displacement Manhattan distance from start to end
 * @param total_movement Cumulative movement during touch
 * @param is_double_tap_pending True if this is second tap in double-tap window
 * @return Tap classification result
 */
trackpad_tap_result_t trackpad_classify_tap(uint32_t duration_ms,
                                            int32_t net_displacement,
                                            int32_t total_movement,
                                            bool is_double_tap_pending);

#ifdef __cplusplus
}
#endif
