/**
 * @file trackpad_gesture.hpp
 * @brief Full-featured trackpad gesture processor
 *
 * Implements click-behavior.md spec:
 * - Default behavior is MOVE (any touch+hold/movement = move)
 * - Tap = short touch AND release (<150ms, <5px)
 * - Multi-tap window chains taps (double/triple/quad click)
 * - Tap-then-hold = drag (click and hold)
 * - Smooth acceleration curve (slow=accurate, fast=accelerate)
 */

#pragma once

#include <cstdint>
#include <cmath>

// ========================== Configuration ==========================

struct TrackpadConfig {
    // Tap detection
    uint32_t tap_max_duration_ms = 150;    // Max duration for a tap
    int32_t tap_max_movement_px = 5;       // Max movement for a tap

    // Multi-tap window
    uint32_t multi_tap_window_ms = 300;    // Window to chain taps together

    // Drag detection
    uint32_t drag_hold_time_ms = 150;      // Hold time after tap to start drag

    // Acceleration curve (smooth power curve)
    float accel_min = 1.5f;                // Multiplier at slow speeds
    float accel_max = 4.0f;                // Multiplier at fast speeds
    float accel_velocity_scale = 100.0f;   // Velocity normalization (px/s)
    float accel_exponent = 0.8f;           // Curve shape (<1 = gentle start)

    // Anti-wiggle (small dead zone for direction changes)
    int32_t anti_wiggle_px = 2;            // Ignore movements smaller than this
};

// ========================== Types ==========================

enum class TouchEvent {
    PRESSED,
    PRESSING,
    RELEASED
};

struct TouchInput {
    TouchEvent event;
    int32_t x;
    int32_t y;
    uint32_t timestamp_ms;
};

// Action types that can be returned
enum class ActionType {
    NONE,
    MOVE,               // Cursor movement (dx, dy)
    CLICK,              // Single click
    DOUBLE_CLICK,       // Double click
    TRIPLE_CLICK,       // Triple click
    QUAD_CLICK,         // Quadruple click
    DRAG_START,         // Begin drag (button down)
    DRAG_MOVE,          // Move while dragging (dx, dy with button held)
    DRAG_END            // End drag (button up)
};

struct TrackpadAction {
    ActionType type = ActionType::NONE;
    int16_t dx = 0;
    int16_t dy = 0;

    TrackpadAction() = default;
    TrackpadAction(ActionType t) : type(t) {}
    TrackpadAction(ActionType t, int16_t x, int16_t y) : type(t), dx(x), dy(y) {}

    bool hasAction() const { return type != ActionType::NONE; }
};

// ========================== Trackpad Class ==========================

class Trackpad {
public:
    Trackpad(uint16_t screen_width, uint16_t screen_height)
        : m_screen_width(screen_width)
        , m_screen_height(screen_height)
    {
        reset();
    }

    /**
     * Process touch input, returns action to execute
     */
    TrackpadAction processInput(const TouchInput& input)
    {
        m_current_time = input.timestamp_ms;

        switch (input.event) {
            case TouchEvent::PRESSED:
                return handlePressed(input);
            case TouchEvent::PRESSING:
                return handlePressing(input);
            case TouchEvent::RELEASED:
                return handleReleased(input);
        }
        return TrackpadAction();
    }

    /**
     * Call periodically to check for time-based state transitions
     * (e.g., tap-hold becoming drag)
     */
    TrackpadAction tick(uint32_t timestamp_ms)
    {
        m_current_time = timestamp_ms;

        // Check if waiting tap should become drag
        if (m_state == State::WAITING_FOR_TAP && m_touch_down) {
            uint32_t hold_time = m_current_time - m_touch_down_time;
            if (hold_time >= m_config.drag_hold_time_ms) {
                // Tap-then-hold = start drag
                m_state = State::DRAGGING;
                m_tap_count = 0;  // Reset tap chain
                return TrackpadAction(ActionType::DRAG_START);
            }
        }

        // Check if tap window expired (emit pending clicks)
        if (m_state == State::WAITING_FOR_TAP && !m_touch_down) {
            uint32_t since_release = m_current_time - m_last_release_time;
            if (since_release >= m_config.multi_tap_window_ms) {
                return emitPendingClicks();
            }
        }

        return TrackpadAction();
    }

    void reset()
    {
        m_state = State::IDLE;
        m_touch_down = false;
        m_tap_count = 0;
        m_last_x = 0;
        m_last_y = 0;
        m_touch_start_x = 0;
        m_touch_start_y = 0;
        m_touch_down_time = 0;
        m_last_release_time = 0;
        m_total_movement = 0;
        m_accum_x = 0.0f;
        m_accum_y = 0.0f;
    }

    TrackpadConfig& config() { return m_config; }
    const TrackpadConfig& config() const { return m_config; }

private:
    enum class State {
        IDLE,           // No touch, no pending taps
        MOVING,         // Touch down, moving cursor
        WAITING_FOR_TAP,// Touch released, might be tap or waiting for next tap
        DRAGGING        // In drag mode (button held)
    };

    // Configuration
    const uint16_t m_screen_width;
    const uint16_t m_screen_height;
    TrackpadConfig m_config;

    // State
    State m_state = State::IDLE;
    bool m_touch_down = false;
    uint8_t m_tap_count = 0;

    // Position tracking
    int32_t m_last_x = 0;
    int32_t m_last_y = 0;
    int32_t m_touch_start_x = 0;
    int32_t m_touch_start_y = 0;

    // Timing
    uint32_t m_current_time = 0;
    uint32_t m_touch_down_time = 0;
    uint32_t m_last_release_time = 0;

    // Movement tracking
    int32_t m_total_movement = 0;

    // Sub-pixel accumulator
    float m_accum_x = 0.0f;
    float m_accum_y = 0.0f;

    // ========================== Event Handlers ==========================

    TrackpadAction handlePressed(const TouchInput& input)
    {
        m_touch_down = true;
        m_last_x = input.x;
        m_last_y = input.y;
        m_touch_start_x = input.x;
        m_touch_start_y = input.y;
        m_touch_down_time = input.timestamp_ms;
        m_total_movement = 0;
        m_accum_x = 0.0f;
        m_accum_y = 0.0f;

        // Check if this is within the multi-tap window
        if (m_state == State::WAITING_FOR_TAP) {
            // Continue in WAITING_FOR_TAP - might become drag or another tap
            return TrackpadAction();
        }

        m_state = State::MOVING;
        return TrackpadAction();
    }

    TrackpadAction handlePressing(const TouchInput& input)
    {
        if (!m_touch_down) {
            return TrackpadAction();
        }

        // Calculate raw delta
        int32_t raw_dx = input.x - m_last_x;
        int32_t raw_dy = input.y - m_last_y;

        // Track total movement (for tap detection)
        m_total_movement += std::abs(raw_dx) + std::abs(raw_dy);

        // Update last position
        m_last_x = input.x;
        m_last_y = input.y;

        // No movement? No action
        if (raw_dx == 0 && raw_dy == 0) {
            return TrackpadAction();
        }

        // Anti-wiggle: ignore very small movements
        if (std::abs(raw_dx) < m_config.anti_wiggle_px &&
            std::abs(raw_dy) < m_config.anti_wiggle_px) {
            return TrackpadAction();
        }

        // If we were waiting for tap and moved significantly, cancel tap detection
        if (m_state == State::WAITING_FOR_TAP &&
            m_total_movement > m_config.tap_max_movement_px) {
            TrackpadAction pending = emitPendingClicks();
            m_state = State::MOVING;
            if (pending.hasAction()) {
                return pending;
            }
        }

        // Simple speed estimate: magnitude of delta (fixed poll rate assumed)
        // Multiply by 200 to approximate px/sec at 200Hz polling
        float speed = (std::abs(raw_dx) + std::abs(raw_dy)) * 200.0f;
        float accel = calculateAcceleration(speed);

        // Accumulate with sub-pixel precision
        m_accum_x += raw_dx * accel;
        m_accum_y += raw_dy * accel;

        // Extract integer part
        int16_t out_dx = static_cast<int16_t>(m_accum_x);
        int16_t out_dy = static_cast<int16_t>(m_accum_y);
        m_accum_x -= out_dx;
        m_accum_y -= out_dy;

        if (out_dx == 0 && out_dy == 0) {
            return TrackpadAction();
        }

        // Return appropriate action based on state
        if (m_state == State::DRAGGING) {
            return TrackpadAction(ActionType::DRAG_MOVE, out_dx, out_dy);
        } else {
            m_state = State::MOVING;
            return TrackpadAction(ActionType::MOVE, out_dx, out_dy);
        }
    }

    TrackpadAction handleReleased(const TouchInput& input)
    {
        if (!m_touch_down) {
            return TrackpadAction();
        }

        m_touch_down = false;
        m_last_release_time = input.timestamp_ms;

        // If dragging, end the drag
        if (m_state == State::DRAGGING) {
            m_state = State::IDLE;
            m_tap_count = 0;
            return TrackpadAction(ActionType::DRAG_END);
        }

        // Check if this qualifies as a tap
        uint32_t duration = input.timestamp_ms - m_touch_down_time;
        int32_t displacement = std::abs(input.x - m_touch_start_x) +
                               std::abs(input.y - m_touch_start_y);

        bool is_tap = (duration <= m_config.tap_max_duration_ms) &&
                      (m_total_movement <= m_config.tap_max_movement_px) &&
                      (displacement <= m_config.tap_max_movement_px);

        if (is_tap) {
            m_tap_count++;
            m_state = State::WAITING_FOR_TAP;
            // Don't emit click yet - wait to see if more taps follow
            return TrackpadAction();
        } else {
            // Not a tap - just a release after movement
            // Emit any pending clicks
            TrackpadAction pending = emitPendingClicks();
            m_state = State::IDLE;
            return pending;
        }
    }

    // ========================== Helpers ==========================

    float calculateAcceleration(float velocity_pps)
    {
        // Simple linear acceleration (fast, no sqrt/pow)
        // accel = min + (max - min) * clamp(velocity / scale, 0, 1)
        float normalized = velocity_pps / m_config.accel_velocity_scale;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        return m_config.accel_min + (m_config.accel_max - m_config.accel_min) * normalized;
    }

    TrackpadAction emitPendingClicks()
    {
        if (m_tap_count == 0) {
            m_state = State::IDLE;
            return TrackpadAction();
        }

        ActionType action_type;
        switch (m_tap_count) {
            case 1:  action_type = ActionType::CLICK; break;
            case 2:  action_type = ActionType::DOUBLE_CLICK; break;
            case 3:  action_type = ActionType::TRIPLE_CLICK; break;
            default: action_type = ActionType::QUAD_CLICK; break;
        }

        m_tap_count = 0;
        m_state = State::IDLE;
        return TrackpadAction(action_type);
    }
};

// Legacy compatibility alias
using MovementAction = TrackpadAction;
