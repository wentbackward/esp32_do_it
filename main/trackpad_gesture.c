/**
 * @file trackpad_gesture.c
 * @brief Pure gesture processing implementation
 */

#include "trackpad_gesture.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// ========================== State Management ==========================

void trackpad_state_init(trackpad_state_t *state,
                         uint16_t hres, uint16_t vres,
                         int32_t scroll_zone_w, int32_t scroll_zone_h)
{
    memset(state, 0, sizeof(*state));
    state->hres = hres;
    state->vres = vres;
    state->scroll_zone_w = scroll_zone_w;
    state->scroll_zone_h = scroll_zone_h;
    state->state = TRACKPAD_STATE_IDLE;
}

void trackpad_state_reset(trackpad_state_t *state)
{
    trackpad_touch_state_t saved_state = state->state;
    uint16_t hres = state->hres;
    uint16_t vres = state->vres;
    int32_t scroll_w = state->scroll_zone_w;
    int32_t scroll_h = state->scroll_zone_h;

    memset(state, 0, sizeof(*state));

    state->hres = hres;
    state->vres = vres;
    state->scroll_zone_w = scroll_w;
    state->scroll_zone_h = scroll_h;
    state->state = TRACKPAD_STATE_IDLE;
}

// ========================== Pure Helper Functions ==========================

int32_t trackpad_clamp_i32(int32_t val, int32_t min, int32_t max)
{
    return (val < min) ? min : (val > max) ? max : val;
}

trackpad_zone_t trackpad_get_zone(int32_t x, int32_t y,
                                  uint16_t hres, uint16_t vres,
                                  int32_t scroll_w, int32_t scroll_h)
{
    bool in_right = (x >= hres - scroll_w);
    bool in_bottom = (y >= vres - scroll_h);

    if (in_right && in_bottom) {
        return TRACKPAD_ZONE_SCROLL_CORNER;
    } else if (in_right) {
        return TRACKPAD_ZONE_SCROLL_V;
    } else if (in_bottom) {
        return TRACKPAD_ZONE_SCROLL_H;
    }
    return TRACKPAD_ZONE_MAIN;
}

int32_t trackpad_filter_jitter(int32_t raw_delta, int32_t threshold)
{
    if (abs(raw_delta) <= threshold) {
        return 0;
    }
    return (raw_delta > 0) ? raw_delta - threshold : raw_delta + threshold;
}

bool trackpad_is_jitter(int32_t dx, int32_t dy, int32_t threshold)
{
    return (abs(dx) <= threshold && abs(dy) <= threshold);
}

float trackpad_ewma_update(float current_smooth, float instant_value, float alpha)
{
    return alpha * instant_value + (1.0f - alpha) * current_smooth;
}

float trackpad_apply_acceleration(float delta, float velocity_pps)
{
    if (fabsf(delta) < 0.5f) {
        return delta;  // Ignore sub-pixel movements
    }

    float multiplier;

    if (velocity_pps < TRACKPAD_ACCEL_PRECISION_THRESHOLD) {
        // Precision zone: sub-unity multiplier for fine control
        multiplier = TRACKPAD_ACCEL_PRECISION_SENSITIVITY;

    } else if (velocity_pps < TRACKPAD_ACCEL_LINEAR_THRESHOLD) {
        // Transition zone: linear interpolation
        float t = (velocity_pps - TRACKPAD_ACCEL_PRECISION_THRESHOLD) /
                  (TRACKPAD_ACCEL_LINEAR_THRESHOLD - TRACKPAD_ACCEL_PRECISION_THRESHOLD);
        multiplier = TRACKPAD_ACCEL_PRECISION_SENSITIVITY +
                     t * (TRACKPAD_ACCEL_BASE_SENSITIVITY - TRACKPAD_ACCEL_PRECISION_SENSITIVITY);

    } else if (velocity_pps < TRACKPAD_ACCEL_MAX_THRESHOLD) {
        // Acceleration zone: sqrt curve for natural feel
        float t = (velocity_pps - TRACKPAD_ACCEL_LINEAR_THRESHOLD) /
                  (TRACKPAD_ACCEL_MAX_THRESHOLD - TRACKPAD_ACCEL_LINEAR_THRESHOLD);
        float curve = sqrtf(t);
        multiplier = TRACKPAD_ACCEL_BASE_SENSITIVITY +
                     curve * (TRACKPAD_ACCEL_MAX_MULTIPLIER - TRACKPAD_ACCEL_BASE_SENSITIVITY);

    } else {
        // Maximum acceleration
        multiplier = TRACKPAD_ACCEL_MAX_MULTIPLIER;
    }

    return delta * multiplier;
}

trackpad_tap_result_t trackpad_classify_tap(uint32_t duration_ms,
                                            int32_t net_displacement,
                                            int32_t total_movement,
                                            bool is_double_tap_pending)
{
    // Too short - likely bounce
    if (duration_ms < TRACKPAD_TAP_MIN_DURATION_MS) {
        return TRACKPAD_TAP_NONE;
    }

    // Too long - it's a hold, not a tap
    if (duration_ms >= TRACKPAD_TAP_MAX_DURATION_MS) {
        return TRACKPAD_TAP_NONE;
    }

    // Detect jitter: high cumulative movement but low net displacement
    bool was_jitter = (total_movement > 30) && (net_displacement < 15);

    // Too much intentional movement
    if (net_displacement >= TRACKPAD_TAP_MOVE_THRESHOLD && !was_jitter) {
        return TRACKPAD_TAP_NONE;
    }

    // It's a tap!
    return is_double_tap_pending ? TRACKPAD_TAP_DOUBLE : TRACKPAD_TAP_SINGLE;
}

// ========================== Gesture Processor ==========================

bool trackpad_process_input(trackpad_state_t *state,
                            const trackpad_input_t *input,
                            trackpad_action_t *action)
{
    // Clear action
    memset(action, 0, sizeof(*action));

    if (input->type == TRACKPAD_EVENT_PRESSED) {
        // ---- Touch Down ----
        state->touch_start.x = input->x;
        state->touch_start.y = input->y;
        state->last_pos.x = input->x;
        state->last_pos.y = input->y;
        state->touch_down_time = input->timestamp_ms;
        state->last_sample_time = input->timestamp_ms;
        state->total_movement = 0;
        state->state = TRACKPAD_STATE_DOWN;
        state->scroll_accum_v = 0.0f;
        state->scroll_accum_h = 0.0f;

        // Check for tap-tap-drag (second tap within window)
        trackpad_zone_t zone = trackpad_get_zone(input->x, input->y,
                                                 state->hres, state->vres,
                                                 state->scroll_zone_w, state->scroll_zone_h);

        if (zone == TRACKPAD_ZONE_MAIN && state->last_tap_time > 0 &&
            (input->timestamp_ms - state->last_tap_time) < TRACKPAD_DOUBLE_TAP_WINDOW_MS) {
            state->tap_tap_pending = true;
            // Show drag indicator
            action->type = TRACKPAD_ACTION_SHOW_DRAG_INDICATOR;
            return true;
        } else {
            state->tap_tap_pending = false;
        }

        return false;  // No action on touch down

    } else if (input->type == TRACKPAD_EVENT_PRESSING) {
        // ---- Touch Moving ----
        int32_t raw_dx = input->x - state->last_pos.x;
        int32_t raw_dy = input->y - state->last_pos.y;

        // Calculate time delta
        uint32_t dt_ms = input->timestamp_ms - state->last_sample_time;
        if (dt_ms == 0) dt_ms = 1;

        // Jitter filtering: ignore very small movements
        if (trackpad_is_jitter(raw_dx, raw_dy, TRACKPAD_JITTER_THRESHOLD)) {
            state->last_pos.x = input->x;
            state->last_pos.y = input->y;
            state->last_sample_time = input->timestamp_ms;
            return false;
        }

        // Apply jitter filter with dead zone
        int32_t filtered_dx = trackpad_filter_jitter(raw_dx, TRACKPAD_JITTER_THRESHOLD);
        int32_t filtered_dy = trackpad_filter_jitter(raw_dy, TRACKPAD_JITTER_THRESHOLD);

        // Calculate instantaneous velocity in pixels per second
        float instant_vx = (float)filtered_dx / (dt_ms * 0.001f);
        float instant_vy = (float)filtered_dy / (dt_ms * 0.001f);

        // Apply EWMA smoothing
        state->velocity_x_smooth = trackpad_ewma_update(state->velocity_x_smooth, instant_vx,
                                                        TRACKPAD_VELOCITY_ALPHA);
        state->velocity_y_smooth = trackpad_ewma_update(state->velocity_y_smooth, instant_vy,
                                                        TRACKPAD_VELOCITY_ALPHA);

        // Calculate velocity magnitude in px/s
        float velocity_pps = sqrtf(state->velocity_x_smooth * state->velocity_x_smooth +
                                   state->velocity_y_smooth * state->velocity_y_smooth);

        // Track total movement
        state->total_movement += abs(raw_dx) + abs(raw_dy);

        // Determine zone
        trackpad_zone_t zone = trackpad_get_zone(state->touch_start.x, state->touch_start.y,
                                                 state->hres, state->vres,
                                                 state->scroll_zone_w, state->scroll_zone_h);

        if (zone == TRACKPAD_ZONE_SCROLL_V || zone == TRACKPAD_ZONE_SCROLL_CORNER) {
            // Vertical scrolling
            state->state = TRACKPAD_STATE_SCROLLING;
            state->scroll_accum_v += (float)filtered_dy / TRACKPAD_SCROLL_SENSITIVITY;

            int8_t scroll_units = (int8_t)state->scroll_accum_v;
            if (scroll_units != 0) {
                action->type = TRACKPAD_ACTION_SCROLL_V;
                action->scroll_v = -scroll_units;  // Invert for natural scrolling
                state->scroll_accum_v -= scroll_units;

                state->last_pos.x = input->x;
                state->last_pos.y = input->y;
                state->last_sample_time = input->timestamp_ms;
                return true;
            }

        } else if (zone == TRACKPAD_ZONE_SCROLL_H) {
            // Horizontal scrolling
            state->state = TRACKPAD_STATE_SCROLLING;
            state->scroll_accum_h += (float)filtered_dx / TRACKPAD_SCROLL_SENSITIVITY;

            int8_t scroll_units = (int8_t)state->scroll_accum_h;
            if (scroll_units != 0) {
                action->type = TRACKPAD_ACTION_SCROLL_H;
                action->scroll_h = scroll_units;
                state->scroll_accum_h -= scroll_units;

                state->last_pos.x = input->x;
                state->last_pos.y = input->y;
                state->last_sample_time = input->timestamp_ms;
                return true;
            }

        } else {
            // Main trackpad area - movement or drag

            // Check for tap-tap-drag initiation
            if (state->tap_tap_pending && state->total_movement > TRACKPAD_DRAG_MOVE_THRESHOLD) {
                if (state->state != TRACKPAD_STATE_DRAGGING) {
                    state->state = TRACKPAD_STATE_DRAGGING;
                    state->button_held = true;
                    action->type = TRACKPAD_ACTION_DRAG_START;
                    action->buttons = 0x01;  // Left button

                    state->last_pos.x = input->x;
                    state->last_pos.y = input->y;
                    state->last_sample_time = input->timestamp_ms;
                    return true;
                }
            } else if (state->total_movement > TRACKPAD_TAP_MOVE_THRESHOLD) {
                state->state = TRACKPAD_STATE_MOVING;
            }

            // Apply acceleration and send movement
            if (filtered_dx != 0 || filtered_dy != 0) {
                float accel_dx = trackpad_apply_acceleration((float)filtered_dx, velocity_pps);
                float accel_dy = trackpad_apply_acceleration((float)filtered_dy, velocity_pps);

                int16_t out_dx = (int16_t)roundf(accel_dx);
                int16_t out_dy = (int16_t)roundf(accel_dy);

                if (state->state == TRACKPAD_STATE_DRAGGING) {
                    action->type = TRACKPAD_ACTION_DRAG_MOVE;
                    action->buttons = 0x01;
                } else {
                    action->type = TRACKPAD_ACTION_MOVE;
                }

                action->dx = out_dx;
                action->dy = out_dy;

                state->last_pos.x = input->x;
                state->last_pos.y = input->y;
                state->last_sample_time = input->timestamp_ms;
                return true;
            }
        }

        state->last_pos.x = input->x;
        state->last_pos.y = input->y;
        state->last_sample_time = input->timestamp_ms;
        return false;

    } else if (input->type == TRACKPAD_EVENT_RELEASED) {
        // ---- Touch Up ----
        uint32_t duration = input->timestamp_ms - state->touch_down_time;

        // Calculate net displacement
        int32_t net_dx = input->x - state->touch_start.x;
        int32_t net_dy = input->y - state->touch_start.y;
        int32_t net_displacement = abs(net_dx) + abs(net_dy);

        // Handle based on state
        if (state->state == TRACKPAD_STATE_DRAGGING) {
            // End drag
            action->type = TRACKPAD_ACTION_DRAG_END;
            action->buttons = 0x00;
            state->button_held = false;
            state->last_tap_time = 0;

            // Hide drag indicator and reset
            state->tap_tap_pending = false;
            state->velocity_x_smooth = 0.0f;
            state->velocity_y_smooth = 0.0f;
            state->state = TRACKPAD_STATE_IDLE;
            return true;

        } else if (state->state == TRACKPAD_STATE_SCROLLING) {
            // End scrolling
            state->last_tap_time = 0;
            state->tap_tap_pending = false;
            state->state = TRACKPAD_STATE_IDLE;
            return false;

        } else {
            // Check for tap
            trackpad_tap_result_t tap_result = trackpad_classify_tap(
                duration, net_displacement, state->total_movement, state->tap_tap_pending);

            if (tap_result == TRACKPAD_TAP_DOUBLE) {
                // Double-click
                action->type = TRACKPAD_ACTION_DOUBLE_CLICK;
                action->buttons = 0x01;
                state->last_tap_time = 0;
                state->tap_tap_pending = false;
                state->velocity_x_smooth = 0.0f;
                state->velocity_y_smooth = 0.0f;
                state->state = TRACKPAD_STATE_IDLE;
                return true;

            } else if (tap_result == TRACKPAD_TAP_SINGLE) {
                // Single click
                action->type = TRACKPAD_ACTION_CLICK_DOWN;
                action->buttons = 0x01;
                state->last_tap_time = input->timestamp_ms;  // Record for tap-tap-drag
                state->velocity_x_smooth = 0.0f;
                state->velocity_y_smooth = 0.0f;
                state->state = TRACKPAD_STATE_IDLE;
                return true;

            } else {
                // Not a tap - just movement
                state->last_tap_time = 0;
                state->tap_tap_pending = false;
                state->velocity_x_smooth = 0.0f;
                state->velocity_y_smooth = 0.0f;
                state->state = TRACKPAD_STATE_IDLE;

                // Hide drag indicator if it was shown
                if (state->tap_tap_pending) {
                    action->type = TRACKPAD_ACTION_HIDE_DRAG_INDICATOR;
                    return true;
                }
                return false;
            }
        }

        // Always hide drag indicator on release
        if (state->tap_tap_pending) {
            state->tap_tap_pending = false;
            // This will be handled by the specific action above
        }
    }

    return false;
}
