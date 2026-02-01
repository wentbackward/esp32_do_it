/**
 * @file trackpad_gesture.hpp
 * @brief Clean C++ trackpad implementation
 */

#pragma once

#include <cstdint>
#include <cmath>

// ========================== Types ==========================

enum class TouchEvent {
    PRESSED,
    PRESSING,
    RELEASED
};

enum class TouchState {
    IDLE,
    DOWN,
    MOVING
};

struct TouchInput {
    TouchEvent event;
    int32_t x;
    int32_t y;
    uint32_t timestamp_ms;
};

struct MovementAction {
    int16_t dx;
    int16_t dy;

    MovementAction() : dx(0), dy(0) {}
    MovementAction(int16_t x, int16_t y) : dx(x), dy(y) {}

    bool hasMovement() const { return dx != 0 || dy != 0; }
};

// ========================== Acceleration Configuration ==========================

struct TrackpadConfig {
    float sensitivity;          // Fixed multiplier for all movements

    TrackpadConfig()
        : sensitivity(2.0f)     // 2x amplification
    {}
};

// ========================== Trackpad Class ==========================

class Trackpad {
public:
    Trackpad(uint16_t screen_width, uint16_t screen_height)
        : m_screen_width(screen_width)
        , m_screen_height(screen_height)
        , m_state(TouchState::IDLE)
        , m_last_x(0)
        , m_last_y(0)
        , m_accum_x(0.0f)
        , m_accum_y(0.0f)
    {}

    // Process touch input, returns movement action if any
    MovementAction processInput(const TouchInput& input)
    {
        switch (input.event) {
            case TouchEvent::PRESSED:
                return handleTouchDown(input);

            case TouchEvent::PRESSING:
                return handleTouchMoving(input);

            case TouchEvent::RELEASED:
                return handleTouchUp(input);
        }

        return MovementAction();
    }

    // Reset to idle state
    void reset()
    {
        m_state = TouchState::IDLE;
        m_accum_x = 0.0f;
        m_accum_y = 0.0f;
    }

    // Configuration
    TrackpadConfig& config() { return m_config; }
    const TrackpadConfig& config() const { return m_config; }

private:
    // Configuration
    const uint16_t m_screen_width;
    const uint16_t m_screen_height;
    TrackpadConfig m_config;

    // State
    TouchState m_state;
    int32_t m_last_x;
    int32_t m_last_y;

    // Sub-pixel accumulators (preserve fractional movements)
    float m_accum_x;
    float m_accum_y;

    // ========================== Event Handlers ==========================

    MovementAction handleTouchDown(const TouchInput& input)
    {
        m_last_x = input.x;
        m_last_y = input.y;
        m_accum_x = 0.0f;
        m_accum_y = 0.0f;
        m_state = TouchState::DOWN;
        return MovementAction();
    }

    MovementAction handleTouchMoving(const TouchInput& input)
    {
        // Calculate raw delta
        int32_t dx = input.x - m_last_x;
        int32_t dy = input.y - m_last_y;

        // Update position
        m_last_x = input.x;
        m_last_y = input.y;

        // No movement
        if (dx == 0 && dy == 0) {
            return MovementAction();
        }

        // Apply fixed sensitivity and accumulate
        m_accum_x += dx * m_config.sensitivity;
        m_accum_y += dy * m_config.sensitivity;

        // Extract integer part, keep fractional for next time
        int16_t out_dx = static_cast<int16_t>(m_accum_x);
        int16_t out_dy = static_cast<int16_t>(m_accum_y);
        m_accum_x -= out_dx;
        m_accum_y -= out_dy;

        m_state = TouchState::MOVING;

        return MovementAction(out_dx, out_dy);
    }

    MovementAction handleTouchUp(const TouchInput& input)
    {
        (void)input;
        m_state = TouchState::IDLE;
        return MovementAction();
    }
};
