/**
 * @file trackpad_gesture.cpp
 * @brief C wrapper for C++ Trackpad class
 */

#include "trackpad_gesture.h"
#include "trackpad_gesture.hpp"
#include <cstdlib>  // for abs

// ========================== C++ to C Wrapper ==========================

// Global trackpad instance (singleton pattern)
static Trackpad* g_trackpad = nullptr;

// ========================== C API Implementation ==========================

void trackpad_state_init(trackpad_state_t *state,
                         uint16_t hres, uint16_t vres,
                         int32_t scroll_zone_w, int32_t scroll_zone_h)
{
    // Create C++ trackpad instance
    if (g_trackpad) {
        delete g_trackpad;
    }
    g_trackpad = new Trackpad(hres, vres);

    // Store config in C state struct for compatibility
    if (state) {
        *state = {};  // Zero-initialize safely
        state->hres = hres;
        state->vres = vres;
        state->scroll_zone_w = scroll_zone_w;
        state->scroll_zone_h = scroll_zone_h;
        state->state = TRACKPAD_STATE_IDLE;
    }
}

void trackpad_state_reset(trackpad_state_t *state)
{
    if (g_trackpad) {
        g_trackpad->reset();
    }

    if (state) {
        state->state = TRACKPAD_STATE_IDLE;
        state->velocity_x_smooth = 0.0f;
        state->velocity_y_smooth = 0.0f;
    }
}

bool trackpad_process_input(trackpad_state_t *state,
                            const trackpad_input_t *input,
                            trackpad_action_t *action)
{
    if (!g_trackpad || !input || !action) {
        return false;
    }

    // Convert C types to C++
    TouchInput cpp_input;
    switch (input->type) {
        case TRACKPAD_EVENT_PRESSED:
            cpp_input.event = TouchEvent::PRESSED;
            break;
        case TRACKPAD_EVENT_PRESSING:
            cpp_input.event = TouchEvent::PRESSING;
            break;
        case TRACKPAD_EVENT_RELEASED:
            cpp_input.event = TouchEvent::RELEASED;
            break;
        default:
            return false;
    }
    cpp_input.x = input->x;
    cpp_input.y = input->y;
    cpp_input.timestamp_ms = input->timestamp_ms;

    // Process through C++ class
    MovementAction movement = g_trackpad->processInput(cpp_input);

    // Convert result back to C - safe zero-initialization
    *action = {};

    if (movement.hasMovement()) {
        action->type = TRACKPAD_ACTION_MOVE;
        action->dx = movement.dx;
        action->dy = movement.dy;
        return true;
    }

    return false;
}

// ========================== Stub implementations for compatibility ==========================

int32_t trackpad_clamp_i32(int32_t val, int32_t min, int32_t max)
{
    return (val < min) ? min : (val > max) ? max : val;
}

trackpad_zone_t trackpad_get_zone(int32_t x, int32_t y,
                                  uint16_t hres, uint16_t vres,
                                  int32_t scroll_w, int32_t scroll_h)
{
    // bool in_right = (x >= hres - scroll_w);
    // bool in_bottom = (y >= vres - scroll_h);

    // if (in_right && in_bottom) return TRACKPAD_ZONE_SCROLL_CORNER;
    // if (in_right) return TRACKPAD_ZONE_SCROLL_V;
    // if (in_bottom) return TRACKPAD_ZONE_SCROLL_H;
    return TRACKPAD_ZONE_MAIN;
}

int32_t trackpad_filter_jitter(int32_t raw_delta, int32_t threshold)
{
    if (abs(raw_delta) <= threshold) return 0;
    return raw_delta;
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
    (void)velocity_pps;
    // Delegate to C++ class
    if (g_trackpad) {
        return g_trackpad->config().sensitivity * delta;
    }
    return delta;
}

trackpad_tap_result_t trackpad_classify_tap(uint32_t duration_ms,
                                            int32_t net_displacement,
                                            int32_t total_movement,
                                            uint8_t tap_count)
{
    // Not implemented - no tap detection
    return TRACKPAD_TAP_NONE;
}
