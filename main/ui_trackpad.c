/**
 * @file ui_trackpad.c
 * @brief Trackpad UI - Touch panel as USB mouse with advanced features
 */

#include "ui_trackpad.h"
#include "app_trackpad.h" // New service
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ui_trackpad";

// ========================== UI Configuration ==========================

// Scroll zone limits now configured via Kconfig:
// CONFIG_APP_HID_TRACKPAD_SCROLL_MIN_PX
// CONFIG_APP_HID_TRACKPAD_SCROLL_MAX_PX

// ========================== Static State ========================== 

// Screen dimensions
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

// ========================== Helper Functions ========================== 

/**
 * @brief Clamp value between min and max
 */
static inline int32_t clamp_i32(int32_t val, int32_t min, int32_t max)
{
    return (val < min) ? min : (val > max) ? max : val;
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

// ========================== UI Update Timer ========================== 

/**
 * @brief Timer callback to update UI cursor from shared state
 * Runs in LVGL task context, so no locking needed
 */
static void ui_update_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_cursor) return;

    // Get status from service
    app_trackpad_status_t status;
    app_trackpad_get_status(&status);

    if (status.touched) {
        lv_obj_set_pos(s_cursor, status.x - 8, status.y - 8);
        lv_obj_clear_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
        
        // Update scroll zone highlights
        if (s_scroll_zone_v) {
             lv_obj_set_style_bg_opa(s_scroll_zone_v, (status.zone == TRACKPAD_ZONE_SCROLL_V) ? LV_OPA_30 : LV_OPA_10, 0);
        }
        if (s_scroll_zone_h) {
             lv_obj_set_style_bg_opa(s_scroll_zone_h, (status.zone == TRACKPAD_ZONE_SCROLL_H) ? LV_OPA_30 : LV_OPA_10, 0);
        }
    } else {
        lv_obj_add_flag(s_cursor, LV_OBJ_FLAG_HIDDEN);
        if (s_scroll_zone_v) lv_obj_set_style_bg_opa(s_scroll_zone_v, LV_OPA_10, 0);
        if (s_scroll_zone_h) lv_obj_set_style_bg_opa(s_scroll_zone_h, LV_OPA_10, 0);
    }

    // Heartbeat for debug
    static uint32_t frame_count = 0;
    if (++frame_count % 30 == 0) {
        // ESP_LOGI(TAG, "Heartbeat... (Touch: %d)", status.touched);
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

    s_hres = cfg->hres;
    s_vres = cfg->vres;
    s_mode_switch_cb = cfg->mode_switch_cb;

    ESP_LOGI(TAG, "Initializing trackpad UI (%%ux%%u)", s_hres, s_vres);

    // Calculate scroll zone dimensions
#if CONFIG_APP_HID_TRACKPAD_SCROLL_ENABLE
    int32_t percent = CONFIG_APP_HID_TRACKPAD_SCROLL_PERCENT;
    int32_t min_px = CONFIG_APP_HID_TRACKPAD_SCROLL_MIN_PX;
    int32_t max_px = CONFIG_APP_HID_TRACKPAD_SCROLL_MAX_PX;

    // Calculate both dimensions
    int32_t scroll_w_calc = clamp_i32((s_hres * percent) / 100, min_px, max_px);
    int32_t scroll_h_calc = clamp_i32((s_vres * percent) / 100, min_px, max_px);

    // Clamp both to the narrowest dimension for consistency
    int32_t scroll_size = (scroll_w_calc < scroll_h_calc) ? scroll_w_calc : scroll_h_calc;
    s_scroll_w = scroll_size;
    s_scroll_h = scroll_size;
#else
    s_scroll_w = 0;
    s_scroll_h = 0;
#endif

    ESP_LOGI(TAG, "Scroll zones: %%%"PRId32"x%%"PRId32" px", s_scroll_w, s_scroll_h);

    // Initialize trackpad service
    app_trackpad_cfg_t service_cfg = {
        .hres = s_hres,
        .vres = s_vres,
        .touch = cfg->touch,
        .hid = cfg->hid,
        .scroll_zone_w = s_scroll_w,
        .scroll_zone_h = s_scroll_h
    };
    app_trackpad_init(&service_cfg);
    
    // Create screen
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Vertical scroll zone (right edge)
    if (s_scroll_w > 0) {
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
    }

    // Horizontal scroll zone (top edge)
    if (s_scroll_h > 0) {
        s_scroll_zone_h = lv_obj_create(scr);
        lv_obj_set_size(s_scroll_zone_h, s_hres - s_scroll_w, s_scroll_h);
        lv_obj_set_pos(s_scroll_zone_h, 0, 0);
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
    }

    // Status label (bottom center)
    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, "Ready");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    // Version label (bottom-right, above scroll zone)
    lv_obj_t *version_label = lv_label_create(scr);
    lv_label_set_text(version_label, CONFIG_APP_VERSION);
    lv_obj_set_style_text_color(version_label, lv_color_hex(0x555555), 0);
    lv_obj_align(version_label, LV_ALIGN_BOTTOM_RIGHT, -(s_scroll_w + 5), -5);

    // Mode switch button (bottom-left corner)
    if (s_mode_switch_cb) {
        s_mode_btn = lv_btn_create(scr);
        lv_obj_set_size(s_mode_btn, 50, 30);
        lv_obj_align(s_mode_btn, LV_ALIGN_BOTTOM_LEFT, 5, -5);
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

    // Bring cursor, drag indicator, and button to front
    lv_obj_move_foreground(s_cursor);
    if (s_mode_btn) {
        lv_obj_move_foreground(s_mode_btn);
    }

    // Start UI update timer (30fps is enough for visual feedback)
    lv_timer_create(ui_update_timer_cb, 33, NULL);

    ESP_LOGI(TAG, "Trackpad UI initialized (%%s)", CONFIG_APP_VERSION);
}

void ui_trackpad_set_mode_switch_cb(ui_trackpad_mode_switch_cb_t cb)
{
    s_mode_switch_cb = cb;
}
