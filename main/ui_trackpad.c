/**
 * @file ui_trackpad.c
 * @brief Trackpad UI - Touch panel as USB mouse with advanced features
 *
 * Features:
 * - Logarithmic acceleration (slow = fine control, fast = large movement)
 * - Tap to click with movement threshold (cancels on swipe)
 * - Click and hold/drag support
 * - Scroll zones (right edge = vertical scroll, bottom edge = horizontal scroll)
 * - Mode toggle button to switch to macro UI
 */

#include "ui_trackpad.h"
#include "app_hid_trackpad.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "ui_trackpad";

// ========================== Configuration ==========================

// Tap detection
#ifndef CONFIG_APP_HID_TRACKPAD_TAP_THRESHOLD_MS
#define TAP_THRESHOLD_MS 200
#else
#define TAP_THRESHOLD_MS CONFIG_APP_HID_TRACKPAD_TAP_THRESHOLD_MS
#endif

// Jitter filtering
#define JITTER_THRESHOLD 3  // Pixels - movements below this are filtered out

// Velocity smoothing (EWMA - Exponentially Weighted Moving Average)
#define VELOCITY_ALPHA 0.3f  // Higher = more responsive, Lower = smoother

// Acceleration curve (velocity in pixels per second)
#define ACCEL_PRECISION_SENSITIVITY 0.5f   // Sub-unity for fine control
#define ACCEL_BASE_SENSITIVITY 1.0f        // Multiplier at transition
#define ACCEL_MAX_MULTIPLIER 5.0f          // Maximum for fast movements
#define ACCEL_PRECISION_THRESHOLD 100.0f   // Below: precision mode (px/s)
#define ACCEL_LINEAR_THRESHOLD 400.0f      // Transition zone (px/s)
#define ACCEL_MAX_THRESHOLD 1500.0f        // Above: max acceleration (px/s)

// Tap detection (net displacement from start point)
#define TAP_MIN_DURATION_MS 50      // Minimum duration to count as tap (debounce)
#define TAP_MOVE_THRESHOLD 15       // Net displacement in pixels (stricter)
#define TAP_JITTER_RATIO 3.0f       // If cumulative/net > this, it's jitter (more lenient)

// Tap-tap-drag timing
#define DOUBLE_TAP_WINDOW_MS 350    // Tighter window
#define DRAG_MOVE_THRESHOLD 25      // More movement needed to start drag

// Scroll zone size (percentage of screen dimension, with min/max pixel limits)
#define SCROLL_ZONE_PERCENT 15
#define SCROLL_ZONE_MIN_PX 24
#define SCROLL_ZONE_MAX_PX 48

// Scroll sensitivity (pixels per scroll unit)
#define SCROLL_SENSITIVITY 20

// ========================== Touch State ==========================

typedef enum {
    TOUCH_STATE_IDLE,
    TOUCH_STATE_DOWN,
    TOUCH_STATE_MOVING,
    TOUCH_STATE_SCROLLING,
    TOUCH_STATE_DRAGGING,
} touch_state_t;

typedef enum {
    ZONE_TRACKPAD,
    ZONE_SCROLL_V,
    ZONE_SCROLL_H,
    ZONE_SCROLL_CORNER,
} zone_t;

// Static state
static app_hid_t *s_hid = NULL;
static uint16_t s_hres = 0;
static uint16_t s_vres = 0;
static int32_t s_scroll_w = 0;  // Calculated scroll zone width
static int32_t s_scroll_h = 0;  // Calculated scroll zone height

// Touch tracking
static touch_state_t s_state = TOUCH_STATE_IDLE;
static lv_point_t s_touch_start = {0, 0};
static lv_point_t s_last_pos = {0, 0};
static uint32_t s_touch_down_time = 0;
static uint32_t s_last_sample_time = 0;
static uint32_t s_last_tap_time = 0;      // For tap-tap-drag detection
static int32_t s_total_movement = 0;
static bool s_button_held = false;
static bool s_tap_tap_pending = false;    // Second tap of tap-tap-drag

// Velocity smoothing (EWMA)
static float s_velocity_x_smooth = 0.0f;
static float s_velocity_y_smooth = 0.0f;

// Scroll accumulator (for sub-unit scroll amounts)
static float s_scroll_accum_v = 0.0f;
static float s_scroll_accum_h = 0.0f;

// UI elements
static lv_obj_t *s_cursor = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_scroll_zone_v = NULL;
static lv_obj_t *s_scroll_zone_h = NULL;
static lv_obj_t *s_mode_btn = NULL;
static lv_obj_t *s_drag_indicator = NULL;

// Mode switch callback
static ui_trackpad_mode_switch_cb_t s_mode_switch_cb = NULL;

// ========================== Helper Functions ==========================

/**
 * @brief Clamp value between min and max
 */
static inline int32_t clamp_i32(int32_t val, int32_t min, int32_t max)
{
    return (val < min) ? min : (val > max) ? max : val;
}

/**
 * @brief Determine which zone a point is in
 */
static zone_t get_zone(int32_t x, int32_t y)
{
    bool in_right = (x >= s_hres - s_scroll_w);
    bool in_bottom = (y >= s_vres - s_scroll_h);

    if (in_right && in_bottom) {
        return ZONE_SCROLL_CORNER;
    } else if (in_right) {
        return ZONE_SCROLL_V;
    } else if (in_bottom) {
        return ZONE_SCROLL_H;
    }
    return ZONE_TRACKPAD;
}

/**
 * @brief Apply dual-phase acceleration curve (precision + speed zones)
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
static float apply_acceleration(float delta, float velocity_pps)
{
    if (fabsf(delta) < 0.5f) {
        return delta;  // Ignore sub-pixel movements
    }

    float multiplier;

    if (velocity_pps < ACCEL_PRECISION_THRESHOLD) {
        // Precision zone: sub-unity multiplier for fine control
        multiplier = ACCEL_PRECISION_SENSITIVITY;

    } else if (velocity_pps < ACCEL_LINEAR_THRESHOLD) {
        // Transition zone: linear interpolation
        float t = (velocity_pps - ACCEL_PRECISION_THRESHOLD) /
                  (ACCEL_LINEAR_THRESHOLD - ACCEL_PRECISION_THRESHOLD);
        multiplier = ACCEL_PRECISION_SENSITIVITY +
                     t * (ACCEL_BASE_SENSITIVITY - ACCEL_PRECISION_SENSITIVITY);

    } else if (velocity_pps < ACCEL_MAX_THRESHOLD) {
        // Acceleration zone: sqrt curve for natural feel
        float t = (velocity_pps - ACCEL_LINEAR_THRESHOLD) /
                  (ACCEL_MAX_THRESHOLD - ACCEL_LINEAR_THRESHOLD);
        float curve = sqrtf(t);  // Gentler than power curve
        multiplier = ACCEL_BASE_SENSITIVITY +
                     curve * (ACCEL_MAX_MULTIPLIER - ACCEL_BASE_SENSITIVITY);

    } else {
        // Maximum acceleration
        multiplier = ACCEL_MAX_MULTIPLIER;
    }

    return delta * multiplier;
}

/**
 * @brief Update status label
 */
static void update_status(const char *text)
{
    if (s_status_label) {
        lv_label_set_text(s_status_label, text);
    }
}

// ========================== Touch Handler ==========================

static void trackpad_touch_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);
    uint32_t now = lv_tick_get();

    if (code == LV_EVENT_PRESSED) {
        // ---- Touch Down ----
        s_touch_start = p;
        s_last_pos = p;
        s_touch_down_time = now;
        s_last_sample_time = now;
        s_total_movement = 0;
        s_state = TOUCH_STATE_DOWN;
        s_scroll_accum_v = 0.0f;
        s_scroll_accum_h = 0.0f;

        // Check for tap-tap-drag (second tap within window of first tap)
        zone_t zone = get_zone(p.x, p.y);
        if (zone == ZONE_TRACKPAD && s_last_tap_time > 0 &&
            (now - s_last_tap_time) < DOUBLE_TAP_WINDOW_MS) {
            s_tap_tap_pending = true;
            // Show visual feedback that drag is ready
            if (s_drag_indicator) {
                lv_obj_set_pos(s_drag_indicator, p.x - 12, p.y - 12);
                lv_obj_clear_flag(s_drag_indicator, LV_OBJ_FLAG_HIDDEN);
            }
            update_status("Drag ready...");
            ESP_LOGD(TAG, "Tap-tap detected, waiting for drag");
        } else {
            s_tap_tap_pending = false;
        }

        // Show cursor
        if (s_cursor) {
            lv_obj_set_pos(s_cursor, p.x - 8, p.y - 8);
            lv_obj_clear_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
        }

        // Highlight scroll zone if in it
        if (zone == ZONE_SCROLL_V && s_scroll_zone_v) {
            lv_obj_set_style_bg_opa(s_scroll_zone_v, LV_OPA_30, 0);
        } else if (zone == ZONE_SCROLL_H && s_scroll_zone_h) {
            lv_obj_set_style_bg_opa(s_scroll_zone_h, LV_OPA_30, 0);
        }

        ESP_LOGD(TAG, "Touch down at (%"PRId32", %"PRId32") zone=%d tap_tap=%d",
                 p.x, p.y, zone, s_tap_tap_pending);

    } else if (code == LV_EVENT_PRESSING) {
        // ---- Touch Moving ----
        int32_t raw_dx = p.x - s_last_pos.x;
        int32_t raw_dy = p.y - s_last_pos.y;

        // Calculate time delta
        uint32_t dt_ms = now - s_last_sample_time;
        if (dt_ms == 0) dt_ms = 1;  // Avoid division by zero

        // Jitter filtering: ignore very small movements
        if (abs(raw_dx) <= JITTER_THRESHOLD && abs(raw_dy) <= JITTER_THRESHOLD) {
            // Below jitter threshold - update tracking but don't move cursor
            s_last_pos = p;
            s_last_sample_time = now;
            return;
        }

        // Subtract dead zone from movement
        int32_t filtered_dx = (raw_dx > JITTER_THRESHOLD) ? raw_dx - JITTER_THRESHOLD :
                              (raw_dx < -JITTER_THRESHOLD) ? raw_dx + JITTER_THRESHOLD : 0;
        int32_t filtered_dy = (raw_dy > JITTER_THRESHOLD) ? raw_dy - JITTER_THRESHOLD :
                              (raw_dy < -JITTER_THRESHOLD) ? raw_dy + JITTER_THRESHOLD : 0;

        // Calculate instantaneous velocity in pixels per second
        float instant_vx = (float)filtered_dx / (dt_ms * 0.001f);
        float instant_vy = (float)filtered_dy / (dt_ms * 0.001f);

        // Apply EWMA smoothing for stable velocity
        s_velocity_x_smooth = VELOCITY_ALPHA * instant_vx +
                              (1.0f - VELOCITY_ALPHA) * s_velocity_x_smooth;
        s_velocity_y_smooth = VELOCITY_ALPHA * instant_vy +
                              (1.0f - VELOCITY_ALPHA) * s_velocity_y_smooth;

        // Calculate velocity magnitude in pixels per second
        float velocity_pps = sqrtf(s_velocity_x_smooth * s_velocity_x_smooth +
                                   s_velocity_y_smooth * s_velocity_y_smooth);

        // Track total movement (for tap/drag detection)
        s_total_movement += abs(raw_dx) + abs(raw_dy);

        // Determine zone and action
        zone_t zone = get_zone(s_touch_start.x, s_touch_start.y);

        if (zone == ZONE_SCROLL_V || zone == ZONE_SCROLL_CORNER) {
            // Vertical scrolling
            s_state = TOUCH_STATE_SCROLLING;
            s_scroll_accum_v += (float)filtered_dy / SCROLL_SENSITIVITY;

            // Send scroll when we have a full unit
            int8_t scroll_units = (int8_t)s_scroll_accum_v;
            if (scroll_units != 0) {
                // Invert for natural scrolling (drag down = scroll down = negative)
                app_hid_trackpad_send_scroll(s_hid, -scroll_units, 0);
                s_scroll_accum_v -= scroll_units;
            }

            update_status("Scrolling V");

        } else if (zone == ZONE_SCROLL_H) {
            // Horizontal scrolling
            s_state = TOUCH_STATE_SCROLLING;
            s_scroll_accum_h += (float)filtered_dx / SCROLL_SENSITIVITY;

            int8_t scroll_units = (int8_t)s_scroll_accum_h;
            if (scroll_units != 0) {
                app_hid_trackpad_send_scroll(s_hid, 0, scroll_units);
                s_scroll_accum_h -= scroll_units;
            }

            update_status("Scrolling H");

        } else {
            // Trackpad movement

            // Check for tap-tap-drag: if this is second tap and we moved, start dragging
            if (s_tap_tap_pending && s_total_movement > DRAG_MOVE_THRESHOLD) {
                if (s_state != TOUCH_STATE_DRAGGING) {
                    // Start drag - press button
                    s_state = TOUCH_STATE_DRAGGING;
                    s_button_held = true;
                    app_hid_trackpad_send_click(s_hid, 0x01);  // Press left button
                    update_status("Dragging");
                    ESP_LOGI(TAG, "Tap-tap-drag started");
                }
            } else if (s_total_movement > TAP_MOVE_THRESHOLD) {
                s_state = TOUCH_STATE_MOVING;
            }

            // Apply acceleration and send movement
            if (filtered_dx != 0 || filtered_dy != 0) {
                float accel_dx = apply_acceleration((float)filtered_dx, velocity_pps);
                float accel_dy = apply_acceleration((float)filtered_dy, velocity_pps);

                int16_t out_dx = (int16_t)roundf(accel_dx);
                int16_t out_dy = (int16_t)roundf(accel_dy);

                if (s_state == TOUCH_STATE_DRAGGING) {
                    // Send movement while holding button
                    app_hid_trackpad_send_report(s_hid, 0x01, out_dx, out_dy, 0, 0);
                } else {
                    // Regular movement
                    app_hid_trackpad_send_move(s_hid, out_dx, out_dy);
                }
            }
        }

        // Update tracking
        s_last_pos = p;
        s_last_sample_time = now;

        // Update cursor position
        if (s_cursor) {
            lv_obj_set_pos(s_cursor, p.x - 8, p.y - 8);
        }

    } else if (code == LV_EVENT_RELEASED) {
        // ---- Touch Up ----
        uint32_t duration = now - s_touch_down_time;

        // Calculate net displacement from start (not cumulative movement)
        int32_t net_dx = p.x - s_touch_start.x;
        int32_t net_dy = p.y - s_touch_start.y;
        int32_t net_displacement = abs(net_dx) + abs(net_dy);  // Manhattan distance

        // Detect jitter: high cumulative movement but low net displacement
        bool was_jitter = (s_total_movement > 30) && (net_displacement < 15);

        ESP_LOGD(TAG, "Touch released: duration=%"PRIu32"ms, net=%"PRId32"px, total=%"PRId32"px, state=%d",
                 duration, net_displacement, s_total_movement, s_state);

        // Handle based on state
        if (s_state == TOUCH_STATE_DRAGGING) {
            // Release drag button
            app_hid_trackpad_send_click(s_hid, 0x00);
            s_button_held = false;
            s_last_tap_time = 0;  // Reset tap tracking after drag
            update_status("Drag complete");
            ESP_LOGI(TAG, "Drag ended");

        } else if (s_state == TOUCH_STATE_SCROLLING) {
            s_last_tap_time = 0;  // Reset tap tracking after scroll
            update_status("Ready");

        } else if (duration >= TAP_MIN_DURATION_MS && duration < TAP_THRESHOLD_MS &&
                   (net_displacement < TAP_MOVE_THRESHOLD || was_jitter)) {
            // This is a tap (deliberate touch with minimal net displacement, or just jitter)
            if (s_tap_tap_pending) {
                // Second tap without drag = double-click
                ESP_LOGI(TAG, "Double-tap detected - sending double-click");
                app_hid_trackpad_send_click(s_hid, 0x01);
                vTaskDelay(pdMS_TO_TICKS(30));
                app_hid_trackpad_send_click(s_hid, 0x00);
                vTaskDelay(pdMS_TO_TICKS(50));
                app_hid_trackpad_send_click(s_hid, 0x01);
                vTaskDelay(pdMS_TO_TICKS(30));
                app_hid_trackpad_send_click(s_hid, 0x00);
                s_last_tap_time = 0;  // Reset after double-click
                update_status("Double-click!");
            } else {
                // First tap - send single click and record time for potential tap-tap
                ESP_LOGI(TAG, "Tap detected (%"PRIu32"ms, net=%"PRId32"px) - sending click",
                         duration, net_displacement);
                app_hid_trackpad_send_click(s_hid, 0x01);
                vTaskDelay(pdMS_TO_TICKS(30));
                app_hid_trackpad_send_click(s_hid, 0x00);
                s_last_tap_time = now;  // Record for tap-tap-drag detection
                update_status("Click!");
            }

        } else {
            s_last_tap_time = 0;  // Movement exceeded threshold, reset tap tracking
            update_status("Ready");
        }

        // Reset tap-tap state and hide drag indicator
        s_tap_tap_pending = false;
        if (s_drag_indicator) {
            lv_obj_add_flag(s_drag_indicator, LV_OBJ_FLAG_HIDDEN);
        }

        // Reset velocity smoothing
        s_velocity_x_smooth = 0.0f;
        s_velocity_y_smooth = 0.0f;

        // Reset state
        s_state = TOUCH_STATE_IDLE;

        // Hide cursor
        if (s_cursor) {
            lv_obj_add_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
        }

        // Reset scroll zone highlights
        if (s_scroll_zone_v) {
            lv_obj_set_style_bg_opa(s_scroll_zone_v, LV_OPA_10, 0);
        }
        if (s_scroll_zone_h) {
            lv_obj_set_style_bg_opa(s_scroll_zone_h, LV_OPA_10, 0);
        }
    }
}

// ========================== Mode Button Handler ==========================

static void mode_btn_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Mode switch requested");
        if (s_mode_switch_cb) {
            s_mode_switch_cb();
        }
    }
}

// ========================== Public API ==========================

void ui_trackpad_init(const trackpad_cfg_t *cfg)
{
    if (!cfg || !cfg->hid) {
        ESP_LOGE(TAG, "Invalid configuration");
        return;
    }

    s_hid = cfg->hid;
    s_hres = cfg->hres;
    s_vres = cfg->vres;
    s_mode_switch_cb = cfg->mode_switch_cb;

    ESP_LOGI(TAG, "Initializing trackpad UI (%ux%u)", s_hres, s_vres);

    // Calculate scroll zone dimensions with min/max clamping
    s_scroll_w = clamp_i32((s_hres * SCROLL_ZONE_PERCENT) / 100,
                           SCROLL_ZONE_MIN_PX, SCROLL_ZONE_MAX_PX);
    s_scroll_h = clamp_i32((s_vres * SCROLL_ZONE_PERCENT) / 100,
                           SCROLL_ZONE_MIN_PX, SCROLL_ZONE_MAX_PX);

    ESP_LOGI(TAG, "Scroll zones: %"PRId32"x%"PRId32" px", s_scroll_w, s_scroll_h);

    // Create screen
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Vertical scroll zone (right edge)
    s_scroll_zone_v = lv_obj_create(scr);
    lv_obj_set_size(s_scroll_zone_v, s_scroll_w, s_vres - s_scroll_h);
    lv_obj_set_pos(s_scroll_zone_v, s_hres - s_scroll_w, 0);
    lv_obj_set_style_bg_color(s_scroll_zone_v, lv_color_hex(0x4a90d9), 0);
    lv_obj_set_style_bg_opa(s_scroll_zone_v, LV_OPA_10, 0);
    lv_obj_set_style_border_width(s_scroll_zone_v, 1, 0);
    lv_obj_set_style_border_color(s_scroll_zone_v, lv_color_hex(0x4a90d9), 0);
    lv_obj_set_style_border_opa(s_scroll_zone_v, LV_OPA_30, 0);
    lv_obj_set_style_radius(s_scroll_zone_v, 0, 0);
    lv_obj_clear_flag(s_scroll_zone_v, LV_OBJ_FLAG_SCROLLABLE);

    // Vertical scroll indicator arrows
    lv_obj_t *scroll_v_label = lv_label_create(s_scroll_zone_v);
    lv_label_set_text(scroll_v_label, LV_SYMBOL_UP "\n\n" LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(scroll_v_label, lv_color_hex(0x4a90d9), 0);
    lv_obj_set_style_text_opa(scroll_v_label, LV_OPA_50, 0);
    lv_obj_center(scroll_v_label);

    // Horizontal scroll zone (bottom edge)
    s_scroll_zone_h = lv_obj_create(scr);
    lv_obj_set_size(s_scroll_zone_h, s_hres - s_scroll_w, s_scroll_h);
    lv_obj_set_pos(s_scroll_zone_h, 0, s_vres - s_scroll_h);
    lv_obj_set_style_bg_color(s_scroll_zone_h, lv_color_hex(0x4a90d9), 0);
    lv_obj_set_style_bg_opa(s_scroll_zone_h, LV_OPA_10, 0);
    lv_obj_set_style_border_width(s_scroll_zone_h, 1, 0);
    lv_obj_set_style_border_color(s_scroll_zone_h, lv_color_hex(0x4a90d9), 0);
    lv_obj_set_style_border_opa(s_scroll_zone_h, LV_OPA_30, 0);
    lv_obj_set_style_radius(s_scroll_zone_h, 0, 0);
    lv_obj_clear_flag(s_scroll_zone_h, LV_OBJ_FLAG_SCROLLABLE);

    // Horizontal scroll indicator arrows
    lv_obj_t *scroll_h_label = lv_label_create(s_scroll_zone_h);
    lv_label_set_text(scroll_h_label, LV_SYMBOL_LEFT "  " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(scroll_h_label, lv_color_hex(0x4a90d9), 0);
    lv_obj_set_style_text_opa(scroll_h_label, LV_OPA_50, 0);
    lv_obj_center(scroll_h_label);

    // Status label (top center)
    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, "Ready");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 5);

    // Version label (top-right, above scroll zone)
    lv_obj_t *version_label = lv_label_create(scr);
    lv_label_set_text(version_label, CONFIG_APP_VERSION);
    lv_obj_set_style_text_color(version_label, lv_color_hex(0x555555), 0);
    lv_obj_align(version_label, LV_ALIGN_TOP_RIGHT, -(s_scroll_w + 5), 5);

    // Mode switch button (top-left corner)
    if (s_mode_switch_cb) {
        s_mode_btn = lv_btn_create(scr);
        lv_obj_set_size(s_mode_btn, 50, 30);
        lv_obj_align(s_mode_btn, LV_ALIGN_TOP_LEFT, 5, 5);
        lv_obj_set_style_bg_color(s_mode_btn, lv_color_hex(0x333355), 0);
        lv_obj_set_style_bg_opa(s_mode_btn, LV_OPA_80, 0);
        lv_obj_add_event_cb(s_mode_btn, mode_btn_handler, LV_EVENT_CLICKED, NULL);

        lv_obj_t *btn_label = lv_label_create(s_mode_btn);
        lv_label_set_text(btn_label, LV_SYMBOL_KEYBOARD);
        lv_obj_center(btn_label);
    }

    // Cursor indicator (16x16 circle, initially hidden)
    s_cursor = lv_obj_create(scr);
    lv_obj_set_size(s_cursor, 16, 16);
    lv_obj_set_style_radius(s_cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_cursor, lv_color_hex(0xff6b6b), 0);
    lv_obj_set_style_bg_opa(s_cursor, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_cursor, 2, 0);
    lv_obj_set_style_border_color(s_cursor, lv_color_hex(0xff6b6b), 0);
    lv_obj_add_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_cursor, LV_OBJ_FLAG_SCROLLABLE);

    // Drag indicator (24x24 green circle, shown when tap-tap-drag pending)
    s_drag_indicator = lv_obj_create(scr);
    lv_obj_set_size(s_drag_indicator, 24, 24);
    lv_obj_set_style_radius(s_drag_indicator, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_drag_indicator, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_bg_opa(s_drag_indicator, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_drag_indicator, 2, 0);
    lv_obj_set_style_border_color(s_drag_indicator, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_flag(s_drag_indicator, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_drag_indicator, LV_OBJ_FLAG_SCROLLABLE);

    // Full-screen transparent touch layer (behind scroll zones visually but captures all events)
    lv_obj_t *touch_layer = lv_obj_create(scr);
    lv_obj_set_size(touch_layer, s_hres, s_vres);
    lv_obj_set_pos(touch_layer, 0, 0);
    lv_obj_set_style_bg_opa(touch_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(touch_layer, 0, 0);
    lv_obj_clear_flag(touch_layer, LV_OBJ_FLAG_SCROLLABLE);

    // Bring cursor, drag indicator, and button to front
    lv_obj_move_foreground(s_cursor);
    lv_obj_move_foreground(s_drag_indicator);
    if (s_mode_btn) {
        lv_obj_move_foreground(s_mode_btn);
    }

    // Add touch event handlers
    lv_obj_add_event_cb(touch_layer, trackpad_touch_handler, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(touch_layer, trackpad_touch_handler, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(touch_layer, trackpad_touch_handler, LV_EVENT_RELEASED, NULL);

    ESP_LOGI(TAG, "Trackpad UI initialized (%s)", CONFIG_APP_VERSION);
}

void ui_trackpad_set_mode_switch_cb(ui_trackpad_mode_switch_cb_t cb)
{
    s_mode_switch_cb = cb;
}
