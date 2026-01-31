/**
 * @file ui_macropad.c
 * @brief Macropad UI - Button grid for USB keyboard macros
 */

#include "ui_macropad.h"
#include "app_hid_macropad.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ui_macropad";

// Maximum supported buttons (4x4 grid)
#define MAX_BUTTONS 16

// Static arrays (following ui_hwtest.c pattern - no malloc)
static lv_obj_t *s_buttons[MAX_BUTTONS];
static char s_button_labels[MAX_BUTTONS][8];  // Label text storage
static uint8_t s_button_modifiers[MAX_BUTTONS];
static uint8_t s_button_keycodes[MAX_BUTTONS];
static uint8_t s_button_count = 0;
static app_hid_t *s_hid = NULL;
static lv_obj_t *s_status_label = NULL;

/**
 * @brief Button click event handler
 */
static void button_click_handler(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code != LV_EVENT_CLICKED) {
        return;
    }

    // Find button index
    uint8_t btn_idx = 0xFF;
    for (uint8_t i = 0; i < s_button_count; i++) {
        if (s_buttons[i] == btn) {
            btn_idx = i;
            break;
        }
    }

    if (btn_idx == 0xFF) {
        ESP_LOGE(TAG, "Button not found in array");
        return;
    }

    uint8_t modifier = s_button_modifiers[btn_idx];
    uint8_t keycode = s_button_keycodes[btn_idx];

    ESP_LOGI(TAG, "Button %u clicked: mod=0x%02X key=0x%02X", btn_idx, modifier, keycode);

    // Send key press
    esp_err_t ret = app_hid_macropad_send_key(s_hid, modifier, keycode);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send key: %s", esp_err_to_name(ret));
        return;
    }

    // Brief delay
    vTaskDelay(pdMS_TO_TICKS(50));

    // Release all keys
    app_hid_macropad_release_all(s_hid);

    // Update status
    if (s_status_label) {
        char status_text[32];
        snprintf(status_text, sizeof(status_text), "Sent: Button %u", btn_idx);
        lv_label_set_text(s_status_label, status_text);
    }
}

void ui_macropad_init(const macropad_cfg_t *cfg)
{
    if (!cfg || !cfg->hid) {
        ESP_LOGE(TAG, "Invalid configuration");
        return;
    }

    s_hid = cfg->hid;
    s_button_count = cfg->button_rows * cfg->button_cols;

    if (s_button_count > MAX_BUTTONS) {
        ESP_LOGE(TAG, "Too many buttons (%u > %u)", s_button_count, MAX_BUTTONS);
        return;
    }

    ESP_LOGI(TAG, "Initializing macropad UI (%ux%u, %ux%u buttons)",
             cfg->hres, cfg->vres, cfg->button_rows, cfg->button_cols);

    // Create screen
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // Title label
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "USB Macropad");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Status label
    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, "Ready");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 35);

    // Calculate button dimensions and spacing
    uint16_t grid_top = 60;
    uint16_t grid_height = cfg->vres - grid_top - 10;
    uint16_t grid_width = cfg->hres - 20;

    uint16_t btn_spacing = 8;
    uint16_t btn_width = (grid_width - (cfg->button_cols - 1) * btn_spacing) / cfg->button_cols;
    uint16_t btn_height = (grid_height - (cfg->button_rows - 1) * btn_spacing) / cfg->button_rows;

    // Create button grid
    for (uint8_t row = 0; row < cfg->button_rows; row++) {
        for (uint8_t col = 0; col < cfg->button_cols; col++) {
            uint8_t btn_idx = row * cfg->button_cols + col;

            // Load key mapping from NVS
            esp_err_t ret = app_hid_macropad_load_mapping(btn_idx,
                                                           &s_button_modifiers[btn_idx],
                                                           &s_button_keycodes[btn_idx]);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to load mapping for button %u", btn_idx);
            }

            // Create button label text (show button number or key)
            snprintf(s_button_labels[btn_idx], sizeof(s_button_labels[btn_idx]), "%u", btn_idx);

            // Create button
            lv_obj_t *btn = lv_btn_create(scr);
            s_buttons[btn_idx] = btn;

            // Position button
            uint16_t x = 10 + col * (btn_width + btn_spacing);
            uint16_t y = grid_top + row * (btn_height + btn_spacing);
            lv_obj_set_pos(btn, x, y);
            lv_obj_set_size(btn, btn_width, btn_height);

            // Style button
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x0088FF), LV_STATE_PRESSED);

            // Add label to button
            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, s_button_labels[btn_idx]);
            lv_obj_center(label);

            // Add click event handler
            lv_obj_add_event_cb(btn, button_click_handler, LV_EVENT_CLICKED, NULL);
        }
    }

    ESP_LOGI(TAG, "Macropad UI initialized with %u buttons", s_button_count);
}
