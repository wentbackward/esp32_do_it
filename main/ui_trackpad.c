/**
 * @file ui_trackpad.c
 * @brief Trackpad UI - Touch panel as USB mouse
 */

#include "ui_trackpad.h"
#include "app_hid_trackpad.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ui_trackpad";

#define DISPLAY_INSTRUCTIONS false

// Tap detection threshold (from Kconfig, default 200ms)
#ifndef CONFIG_APP_HID_TRACKPAD_TAP_THRESHOLD_MS
#define TAP_THRESHOLD_MS 200
#else
#define TAP_THRESHOLD_MS CONFIG_APP_HID_TRACKPAD_TAP_THRESHOLD_MS
#endif

// Sensitivity multiplier (from Kconfig, default 2)
#ifndef CONFIG_APP_HID_TRACKPAD_SENSITIVITY
#define SENSITIVITY 2
#else
#define SENSITIVITY CONFIG_APP_HID_TRACKPAD_SENSITIVITY
#endif

// Static state
static app_hid_t *s_hid = NULL;
static lv_point_t s_last_pos = {0, 0};
static uint32_t s_touch_down_time = 0;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *instructions = NULL;
static lv_obj_t *s_cursor = NULL;

/**
 * @brief Touch event handler for trackpad
 */
static void trackpad_touch_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    // lv_indev_t *indev = lv_event_get_target(e);
    lv_indev_t *indev = lv_indev_active();
    // if (!indev) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        // Record initial position and time
        s_last_pos = p;
        s_touch_down_time = lv_tick_get();

        // Update cursor position
        if (s_cursor) {
            lv_obj_set_pos(s_cursor, p.x - 8, p.y - 8);
            lv_obj_clear_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
        }

        ESP_LOGD(TAG, "Touch down at (%d, %d)", p.x, p.y);

    } else if (code == LV_EVENT_PRESSING) {
        // Calculate delta from last position
        int16_t dx = (p.x - s_last_pos.x) * SENSITIVITY;
        int16_t dy = (p.y - s_last_pos.y) * SENSITIVITY;

        if (dx != 0 || dy != 0) {
            // Send mouse movement
            esp_err_t ret = app_hid_trackpad_send_move(s_hid, dx, dy);
            if (ret == ESP_OK) {
                s_last_pos = p;
                #if DISPLAY_INSTRUCTIONS
                ESP_LOGD(TAG, "Move delta: (%d, %d)", dx, dy);
                #endif
            }
        }

        // Update cursor position
        if (s_cursor) {
            lv_obj_set_pos(s_cursor, p.x - 8, p.y - 8);
        }
        // Update status
        #if DISPLAY_INSTRUCTIONS
        if (instructions) {
            char buf[80];
            snprintf(buf, sizeof(buf),
                 "Touch: x=%" PRId32 " y=%" PRId32 " (%s)",
                 p.x, p.y, (code == LV_EVENT_RELEASED) ? "up" : "down");
            lv_label_set_text(instructions, buf);
        }
        #endif

    } else if (code == LV_EVENT_RELEASED) {
        // Check if this was a tap (short duration)
        uint32_t duration = lv_tick_get() - s_touch_down_time;

        if (duration < TAP_THRESHOLD_MS) {
            ESP_LOGI(TAG, "Tap detected (%lu ms) - sending click", duration);

            // Send left click (press)
            app_hid_trackpad_send_click(s_hid, 0x01);

            // Wait briefly
            vTaskDelay(pdMS_TO_TICKS(50));

            // Release click
            app_hid_trackpad_send_click(s_hid, 0x00);

            // Update status
            if (s_status_label) {
                lv_label_set_text(s_status_label, "Status: Click sent");
            }
        } else {
            // Update status
            if (s_status_label) {
                lv_label_set_text(s_status_label, "Status: Ready");
            }
        }

        // Hide cursor
        if (s_cursor) {
            lv_obj_add_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
        }

        ESP_LOGD(TAG, "Touch released");
    }
}

void ui_trackpad_init(const trackpad_cfg_t *cfg)
{
    if (!cfg || !cfg->hid) {
        ESP_LOGE(TAG, "Invalid configuration");
        return;
    }

    s_hid = cfg->hid;

    ESP_LOGI(TAG, "Initializing trackpad UI (%ux%u)", cfg->hres, cfg->vres);

    // Create screen
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // Title label
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "USB Trackpad");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Status label
    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, "Status: Ready");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 35);

    // Instructions label
    #if DISPLAY_INSTRUCTION
    instructions = lv_label_create(scr);
    lv_label_set_text(instructions, "Drag to move cursor\nTap to click");
    lv_obj_set_style_text_color(instructions, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(instructions, LV_ALIGN_CENTER, 0, 0);
    #endif

    // Cursor indicator (16x16 circle, initially hidden)
    s_cursor = lv_obj_create(scr);
    lv_obj_set_size(s_cursor, 16, 16);
    lv_obj_set_style_radius(s_cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_cursor, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(s_cursor, LV_OPA_50, 0);
    lv_obj_set_style_border_width(s_cursor, 2, 0);
    lv_obj_set_style_border_color(s_cursor, lv_color_hex(0xFF0000), 0);
    lv_obj_add_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);

    // Full-screen transparent touch layer
    lv_obj_t *touch_layer = lv_obj_create(scr);
    lv_obj_set_size(touch_layer, cfg->hres, cfg->vres);
    lv_obj_set_pos(touch_layer, 0, 0);
    lv_obj_set_style_bg_opa(touch_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(touch_layer, 0, 0);

    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_status_label, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_cursor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(touch_layer, LV_OBJ_FLAG_SCROLLABLE);

    // Add touch event handler to LVGL input device
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev) {
        lv_obj_add_event_cb(touch_layer, trackpad_touch_handler, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(touch_layer, trackpad_touch_handler, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(touch_layer, trackpad_touch_handler, LV_EVENT_RELEASED, NULL);
    }

    ESP_LOGI(TAG, "Trackpad UI initialized");
}
