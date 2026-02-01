/**
 * @file ui_trackpad.c
 * @brief Trackpad UI - Touch panel as USB mouse with advanced features
 *
 * Features:
 * - Dual-phase acceleration (slow = fine control, fast = large movement)
 * - Tap to click with strict threshold (150ms, 2px)
 * - Tap-hold-drag support (tap, then hold to drag)
 * - Multi-tap detection (double/triple/quadruple click)
 * - Scroll zones (right edge = vertical scroll, bottom edge = horizontal scroll)
 * - Mode toggle button to switch to macro UI
 *
 * Architecture:
 * - Uses trackpad_gesture.c for all gesture processing (tested module)
 * - This file is a thin LVGL adapter layer
 */

#include "ui_trackpad.h"
#include "trackpad_gesture.h"  // Tested gesture processing module
#include "app_hid_trackpad.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "ui_trackpad";

// ========================== UI Configuration ==========================

// Scroll zone size (percentage of screen dimension, with min/max pixel limits)
#define SCROLL_ZONE_PERCENT 10
#define SCROLL_ZONE_MIN_PX 12
#define SCROLL_ZONE_MAX_PX 24

// Note: All gesture parameters (tap thresholds, acceleration, etc.) are now
// defined in trackpad_gesture.h and used by the tested gesture processor

// ========================== Static State ==========================

// HID interface
static app_hid_t *s_hid = NULL;

// Touch handle for direct polling (bypasses LVGL for HID)
static esp_lcd_touch_handle_t s_touch = NULL;

// Gesture processor state (uses tested trackpad_gesture module)
static trackpad_state_t s_gesture_state;

// Screen dimensions (stored separately for UI)
static uint16_t s_hres = 0;
static uint16_t s_vres = 0;
static int32_t s_scroll_w = 0;
static int32_t s_scroll_h = 0;

// UI elements
static lv_obj_t *s_cursor = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_scroll_zone_v = NULL;
static lv_obj_t *s_scroll_zone_h = NULL;
static lv_obj_t *s_mode_btn = NULL;

// Mode switch callback
static ui_trackpad_mode_switch_cb_t s_mode_switch_cb = NULL;

// Touch polling task handle
static TaskHandle_t s_touch_task = NULL;

// ========================== Helper Functions ==========================

/**
 * @brief Clamp value between min and max
 */
static inline int32_t clamp_i32(int32_t val, int32_t min, int32_t max)
{
    return (val < min) ? min : (val > max) ? max : val;
}

// ========================== Helper Functions ==========================

/**
 * @brief Update status label
 */
static void update_status(const char *text)
{
    if (s_status_label) {
        lv_label_set_text(s_status_label, text);
    }
}

// ========================== Touch Polling Task (High Frequency HID) ==========================

// Touch state for polling task
static int32_t s_last_x = 0;
static int32_t s_last_y = 0;
static bool s_was_touched = false;

// Shared state for UI (updated by polling task, read by LVGL)
static volatile int32_t s_ui_x = 0;
static volatile int32_t s_ui_y = 0;
static volatile bool s_ui_touched = false;

// ========================== Click Queue (Non-blocking) ==========================

// Pending click state (processed over multiple poll cycles)
static volatile uint8_t s_pending_clicks = 0;      // Number of clicks to send
static volatile uint8_t s_click_phase = 0;          // 0=idle, 1=pressed, 2=released
static volatile uint32_t s_click_time = 0;          // Time of last click phase change

#define CLICK_PRESS_MS   10   // How long to hold button down
#define CLICK_GAP_MS     30   // Gap between clicks in multi-click

/**
 * @brief Process pending clicks (called each poll cycle, non-blocking)
 */
static void process_pending_clicks(uint32_t now)
{
    if (s_pending_clicks == 0) return;

    uint32_t elapsed = now - s_click_time;

    if (s_click_phase == 0) {
        // Start a click - press down
        app_hid_trackpad_send_click(s_hid, 0x01);
        s_click_phase = 1;
        s_click_time = now;
    } else if (s_click_phase == 1 && elapsed >= CLICK_PRESS_MS) {
        // Release button
        app_hid_trackpad_send_click(s_hid, 0x00);
        s_click_phase = 2;
        s_click_time = now;
        s_pending_clicks--;
    } else if (s_click_phase == 2 && s_pending_clicks > 0 && elapsed >= CLICK_GAP_MS) {
        // Start next click
        s_click_phase = 0;
    } else if (s_click_phase == 2 && s_pending_clicks == 0) {
        // Done with all clicks
        s_click_phase = 0;
    }
}

/**
 * @brief Queue clicks (non-blocking)
 */
static void queue_clicks(uint8_t count, uint32_t now)
{
    s_pending_clicks = count;
    s_click_phase = 0;
    s_click_time = now;
}

/**
 * @brief Execute HID action based on gesture processor output (non-blocking)
 */
static void execute_action(const trackpad_action_t *action, uint32_t now)
{
    switch (action->type) {
        case TRACKPAD_ACTION_MOVE:
            app_hid_trackpad_send_move(s_hid, action->dx, action->dy);
            break;

        case TRACKPAD_ACTION_CLICK_DOWN:
            queue_clicks(1, now);
            break;

        case TRACKPAD_ACTION_DOUBLE_CLICK:
            queue_clicks(2, now);
            break;

        case TRACKPAD_ACTION_TRIPLE_CLICK:
            queue_clicks(3, now);
            break;

        case TRACKPAD_ACTION_QUADRUPLE_CLICK:
            queue_clicks(4, now);
            break;

        case TRACKPAD_ACTION_DRAG_START:
            app_hid_trackpad_send_click(s_hid, 0x01);
            break;

        case TRACKPAD_ACTION_DRAG_MOVE:
            app_hid_trackpad_send_report(s_hid, 0x01, action->dx, action->dy, 0, 0);
            break;

        case TRACKPAD_ACTION_DRAG_END:
            app_hid_trackpad_send_click(s_hid, 0x00);
            break;

        default:
            break;
    }
}

/**
 * @brief Get current timestamp in milliseconds
 */
static uint32_t get_timestamp_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief High-frequency touch polling task
 * Reads touch directly from hardware, processes gestures, sends HID at ~200Hz
 * Completely independent of LVGL refresh rate
 */
static void touch_poll_task(void *arg)
{
    (void)arg;
    const TickType_t poll_interval = pdMS_TO_TICKS(5);  // 200Hz

    // Wait for system to stabilize
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        if (!s_touch || !s_hid) {
            vTaskDelay(poll_interval);
            continue;
        }

        uint32_t now = get_timestamp_ms();

        // Read touch directly from hardware
        esp_lcd_touch_read_data(s_touch);

        uint16_t x, y, strength;
        uint8_t point_num = 0;
        bool touched = esp_lcd_touch_get_coordinates(s_touch, &x, &y, &strength, &point_num, 1);

        // Process through gesture processor
        trackpad_input_t input;
        trackpad_action_t action;

        if (touched && !s_was_touched) {
            // Touch down
            input.type = TRACKPAD_EVENT_PRESSED;
            input.x = x;
            input.y = y;
            input.timestamp_ms = now;
            if (trackpad_process_input(&s_gesture_state, &input, &action)) {
                execute_action(&action, now);
            }

            // Show cursor
            if (s_cursor && lvgl_port_lock(10)) {
                lv_obj_set_pos(s_cursor, x - 8, y - 8);
                lv_obj_clear_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
                lvgl_port_unlock();
            }

        } else if (touched && s_was_touched) {
            // Touch moving
            input.type = TRACKPAD_EVENT_PRESSING;
            input.x = x;
            input.y = y;
            input.timestamp_ms = now;
            if (trackpad_process_input(&s_gesture_state, &input, &action)) {
                execute_action(&action, now);
            }

            // Update cursor position
            if (s_cursor && lvgl_port_lock(10)) {
                lv_obj_set_pos(s_cursor, x - 8, y - 8);
                lvgl_port_unlock();
            }

        } else if (!touched && s_was_touched) {
            // Touch released
            input.type = TRACKPAD_EVENT_RELEASED;
            input.x = s_last_x;
            input.y = s_last_y;
            input.timestamp_ms = now;
            if (trackpad_process_input(&s_gesture_state, &input, &action)) {
                execute_action(&action, now);
            }

            // Hide cursor
            if (s_cursor && lvgl_port_lock(10)) {
                lv_obj_add_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
                lvgl_port_unlock();
            }
        }

        // Tick for time-based transitions (tap window expiry, drag detection)
        if (trackpad_tick(now, &action)) {
            execute_action(&action, now);
        }

        // Process pending clicks (non-blocking)
        process_pending_clicks(now);

        // Update state
        if (touched) {
            s_last_x = x;
            s_last_y = y;
        }
        s_was_touched = touched;

        vTaskDelay(poll_interval);
    }
}

// ========================== LVGL Touch Handler (UI Only) ==========================

/**
 * @brief LVGL event handler - UI updates only, no HID
 * HID is handled by the separate polling task
 */
static void trackpad_touch_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    // Get touch point from LVGL for UI updates
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);
    int32_t px = p.x;
    int32_t py = p.y;

    // Dummy to keep compiler happy (gesture code not used)
    trackpad_input_t input = {};
    trackpad_action_t action = {};
    bool has_action = false;
    (void)input;
    (void)action;

    if (has_action) {
        switch (action.type) {
            case TRACKPAD_ACTION_MOVE:
                // Already handled by polling task
                break;

            case TRACKPAD_ACTION_CLICK_DOWN:
                app_hid_trackpad_send_click(s_hid, 0x01);
                vTaskDelay(pdMS_TO_TICKS(30));
                app_hid_trackpad_send_click(s_hid, 0x00);
                update_status("Click!");
                ESP_LOGI(TAG, "Click");
                break;

            case TRACKPAD_ACTION_DOUBLE_CLICK:
                app_hid_trackpad_send_click(s_hid, 0x01);
                vTaskDelay(pdMS_TO_TICKS(30));
                app_hid_trackpad_send_click(s_hid, 0x00);
                vTaskDelay(pdMS_TO_TICKS(50));
                app_hid_trackpad_send_click(s_hid, 0x01);
                vTaskDelay(pdMS_TO_TICKS(30));
                app_hid_trackpad_send_click(s_hid, 0x00);
                update_status("Double-click!");
                ESP_LOGI(TAG, "Double-click");
                break;

            case TRACKPAD_ACTION_TRIPLE_CLICK:
                // Triple click sequence
                for (int i = 0; i < 3; i++) {
                    app_hid_trackpad_send_click(s_hid, 0x01);
                    vTaskDelay(pdMS_TO_TICKS(30));
                    app_hid_trackpad_send_click(s_hid, 0x00);
                    if (i < 2) vTaskDelay(pdMS_TO_TICKS(50));
                }
                update_status("Triple-click!");
                ESP_LOGI(TAG, "Triple-click");
                break;

            case TRACKPAD_ACTION_QUADRUPLE_CLICK:
                // Quadruple click sequence
                for (int i = 0; i < 4; i++) {
                    app_hid_trackpad_send_click(s_hid, 0x01);
                    vTaskDelay(pdMS_TO_TICKS(30));
                    app_hid_trackpad_send_click(s_hid, 0x00);
                    if (i < 3) vTaskDelay(pdMS_TO_TICKS(50));
                }
                update_status("Quad-click!");
                ESP_LOGI(TAG, "Quadruple-click");
                break;

            case TRACKPAD_ACTION_SCROLL_V:
                app_hid_trackpad_send_scroll(s_hid, action.scroll_v, 0);
                update_status("Scrolling V");
                break;

            case TRACKPAD_ACTION_SCROLL_H:
                app_hid_trackpad_send_scroll(s_hid, 0, action.scroll_h);
                update_status("Scrolling H");
                break;

            case TRACKPAD_ACTION_DRAG_START:
                app_hid_trackpad_send_click(s_hid, 0x01);  // Press and hold
                update_status("Dragging");
                ESP_LOGI(TAG, "Drag started");
                break;

            case TRACKPAD_ACTION_DRAG_MOVE:
                app_hid_trackpad_send_report(s_hid, 0x01, action.dx, action.dy, 0, 0);
                break;

            case TRACKPAD_ACTION_DRAG_END:
                app_hid_trackpad_send_click(s_hid, 0x00);  // Release button
                update_status("Drag complete");
                ESP_LOGI(TAG, "Drag ended");
                break;

            default:
                break;
        }
    }

    // Update UI elements based on touch state
    if (code == LV_EVENT_PRESSED) {
        // Show cursor
        if (s_cursor) {
            lv_obj_set_pos(s_cursor, px - 8, py - 8);
            lv_obj_clear_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
        }

        // Highlight scroll zones
        trackpad_zone_t zone = trackpad_get_zone(px, py, s_hres, s_vres, s_scroll_w, s_scroll_h);
        if (zone == TRACKPAD_ZONE_SCROLL_V && s_scroll_zone_v) {
            lv_obj_set_style_bg_opa(s_scroll_zone_v, LV_OPA_30, 0);
        } else if (zone == TRACKPAD_ZONE_SCROLL_H && s_scroll_zone_h) {
            lv_obj_set_style_bg_opa(s_scroll_zone_h, LV_OPA_30, 0);
        }

    } else if (code == LV_EVENT_PRESSING) {
        // Update cursor position
        if (s_cursor) {
            lv_obj_set_pos(s_cursor, px - 8, py - 8);
        }

    } else if (code == LV_EVENT_RELEASED) {
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

        update_status("Ready");
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
    s_touch = cfg->touch;
    s_hres = cfg->hres;
    s_vres = cfg->vres;
    s_mode_switch_cb = cfg->mode_switch_cb;

    ESP_LOGI(TAG, "Initializing trackpad UI (%ux%u)", s_hres, s_vres);

    // Calculate scroll zone dimensions with min/max clamping
    s_scroll_w = trackpad_clamp_i32((s_hres * SCROLL_ZONE_PERCENT) / 100,
                                    SCROLL_ZONE_MIN_PX, SCROLL_ZONE_MAX_PX);
    s_scroll_h = trackpad_clamp_i32((s_vres * SCROLL_ZONE_PERCENT) / 100,
                                    SCROLL_ZONE_MIN_PX, SCROLL_ZONE_MAX_PX);

    ESP_LOGI(TAG, "Scroll zones: %"PRId32"x%"PRId32" px", s_scroll_w, s_scroll_h);

    // Initialize gesture processor state
    trackpad_state_init(&s_gesture_state, s_hres, s_vres, s_scroll_w, s_scroll_h);
    ESP_LOGI(TAG, "Gesture processor initialized (tap: %dms/%dpx, multi-tap window: %dms)",
             TRACKPAD_TAP_MAX_DURATION_MS, TRACKPAD_TAP_MOVE_THRESHOLD, TRACKPAD_MULTI_TAP_WINDOW_MS);

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

    // Full-screen transparent touch layer (behind scroll zones visually but captures all events)
    lv_obj_t *touch_layer = lv_obj_create(scr);
    lv_obj_set_size(touch_layer, s_hres, s_vres);
    lv_obj_set_pos(touch_layer, 0, 0);
    lv_obj_set_style_bg_opa(touch_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(touch_layer, 0, 0);
    lv_obj_clear_flag(touch_layer, LV_OBJ_FLAG_SCROLLABLE);

    // Bring cursor, drag indicator, and button to front
    lv_obj_move_foreground(s_cursor);
    if (s_mode_btn) {
        lv_obj_move_foreground(s_mode_btn);
    }

    // Add touch event handlers (only used if LVGL has touch - not in trackpad mode)
    lv_obj_add_event_cb(touch_layer, trackpad_touch_handler, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(touch_layer, trackpad_touch_handler, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(touch_layer, trackpad_touch_handler, LV_EVENT_RELEASED, NULL);

    // Start high-frequency touch polling task AFTER UI is created
    if (s_touch) {
        xTaskCreate(touch_poll_task, "touch_poll", 4096, NULL, 3, &s_touch_task);
        ESP_LOGI(TAG, "Touch polling task started (200Hz)");
    }

    ESP_LOGI(TAG, "Trackpad UI initialized (%s)", CONFIG_APP_VERSION);
}

void ui_trackpad_set_mode_switch_cb(ui_trackpad_mode_switch_cb_t cb)
{
    s_mode_switch_cb = cb;
}
