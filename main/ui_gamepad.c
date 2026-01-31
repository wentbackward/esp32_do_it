/**
 * @file ui_gamepad.c
 * @brief Gamepad UI - D-pad and action buttons for USB gamepad
 */

#include "ui_gamepad.h"
#include "app_hid_gamepad.h"
#include "esp_log.h"

static const char *TAG = "ui_gamepad";

// Static state (following project pattern - no malloc)
static app_hid_t *s_hid = NULL;
static gamepad_state_t s_state = {0, 0, 0};  // X, Y, buttons
static lv_obj_t *s_status_label = NULL;

/**
 * @brief Send current gamepad state to host
 */
static void send_gamepad_state(void)
{
    esp_err_t ret = app_hid_gamepad_send_state(s_hid, &s_state);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send gamepad state: %s", esp_err_to_name(ret));
    }

    // Update status label
    if (s_status_label) {
        char status_text[64];
        snprintf(status_text, sizeof(status_text), "X:%d Y:%d Btns:0x%02X",
                 s_state.x, s_state.y, s_state.buttons);
        lv_label_set_text(s_status_label, status_text);
    }
}

/**
 * @brief D-pad button event handler
 */
static void dpad_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    int8_t *axis_ptr = (int8_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        // Set axis value based on button
        if (axis_ptr == &s_state.x) {
            // Left/Right button
            const char *label = lv_label_get_text(lv_obj_get_child(btn, 0));
            s_state.x = (label[0] == 'L') ? -1 : 1;
        } else if (axis_ptr == &s_state.y) {
            // Up/Down button
            const char *label = lv_label_get_text(lv_obj_get_child(btn, 0));
            s_state.y = (label[0] == 'U') ? -1 : 1;
        }
        send_gamepad_state();
        ESP_LOGD(TAG, "D-pad pressed: X=%d Y=%d", s_state.x, s_state.y);

    } else if (code == LV_EVENT_RELEASED) {
        // Reset axis value
        if (axis_ptr == &s_state.x) {
            s_state.x = 0;
        } else if (axis_ptr == &s_state.y) {
            s_state.y = 0;
        }
        send_gamepad_state();
        ESP_LOGD(TAG, "D-pad released: X=%d Y=%d", s_state.x, s_state.y);
    }
}

/**
 * @brief Action button event handler
 */
static void action_button_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    uint8_t button_bit = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        // Set button bit
        s_state.buttons |= button_bit;
        send_gamepad_state();
        ESP_LOGD(TAG, "Button pressed: 0x%02X (total: 0x%02X)", button_bit, s_state.buttons);

    } else if (code == LV_EVENT_RELEASED) {
        // Clear button bit
        s_state.buttons &= ~button_bit;
        send_gamepad_state();
        ESP_LOGD(TAG, "Button released: 0x%02X (total: 0x%02X)", button_bit, s_state.buttons);
    }
}

/**
 * @brief Create a styled button
 */
static lv_obj_t *create_button(lv_obj_t *parent, const char *label_text,
                                 int16_t x, int16_t y, uint16_t w, uint16_t h,
                                 lv_color_t color)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, color, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_lighten(color, 50), LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, label_text);
    lv_obj_center(label);

    return btn;
}

void ui_gamepad_init(const gamepad_cfg_t *cfg)
{
    if (!cfg || !cfg->hid) {
        ESP_LOGE(TAG, "Invalid configuration");
        return;
    }

    s_hid = cfg->hid;

    ESP_LOGI(TAG, "Initializing gamepad UI (%ux%u)", cfg->hres, cfg->vres);

    // Create screen
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // Title label
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "USB Gamepad");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Status label
    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, "X:0 Y:0 Btns:0x00");
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 35);

    // Calculate button dimensions
    uint16_t btn_size = 60;
    uint16_t dpad_spacing = 10;
    uint16_t dpad_center_x = cfg->hres / 4;
    uint16_t dpad_center_y = cfg->vres / 2 + 20;

    // D-pad buttons (Up, Down, Left, Right)
    lv_color_t dpad_color = lv_color_hex(0x444444);

    // Up button
    lv_obj_t *btn_up = create_button(scr, "UP",
                                      dpad_center_x - btn_size / 2,
                                      dpad_center_y - btn_size - dpad_spacing,
                                      btn_size, btn_size, dpad_color);
    lv_obj_add_event_cb(btn_up, dpad_event_handler, LV_EVENT_PRESSED, &s_state.y);
    lv_obj_add_event_cb(btn_up, dpad_event_handler, LV_EVENT_RELEASED, &s_state.y);

    // Down button
    lv_obj_t *btn_down = create_button(scr, "DN",
                                        dpad_center_x - btn_size / 2,
                                        dpad_center_y + dpad_spacing,
                                        btn_size, btn_size, dpad_color);
    lv_obj_add_event_cb(btn_down, dpad_event_handler, LV_EVENT_PRESSED, &s_state.y);
    lv_obj_add_event_cb(btn_down, dpad_event_handler, LV_EVENT_RELEASED, &s_state.y);

    // Left button
    lv_obj_t *btn_left = create_button(scr, "LT",
                                        dpad_center_x - btn_size - dpad_spacing,
                                        dpad_center_y - btn_size / 2,
                                        btn_size, btn_size, dpad_color);
    lv_obj_add_event_cb(btn_left, dpad_event_handler, LV_EVENT_PRESSED, &s_state.x);
    lv_obj_add_event_cb(btn_left, dpad_event_handler, LV_EVENT_RELEASED, &s_state.x);

    // Right button
    lv_obj_t *btn_right = create_button(scr, "RT",
                                         dpad_center_x + dpad_spacing,
                                         dpad_center_y - btn_size / 2,
                                         btn_size, btn_size, dpad_color);
    lv_obj_add_event_cb(btn_right, dpad_event_handler, LV_EVENT_PRESSED, &s_state.x);
    lv_obj_add_event_cb(btn_right, dpad_event_handler, LV_EVENT_RELEASED, &s_state.x);

    // Action buttons (A, B, X, Y) on right side
    uint16_t action_center_x = cfg->hres * 3 / 4;
    uint16_t action_center_y = dpad_center_y;
    uint16_t action_btn_size = 60;
    uint16_t action_spacing = 10;

    // Button A (bottom)
    lv_obj_t *btn_a = create_button(scr, "A",
                                     action_center_x - action_btn_size / 2,
                                     action_center_y + action_spacing,
                                     action_btn_size, action_btn_size,
                                     lv_color_hex(0x00AA00));
    lv_obj_add_event_cb(btn_a, action_button_event_handler, LV_EVENT_PRESSED,
                        (void *)(uintptr_t)GAMEPAD_BTN_A);
    lv_obj_add_event_cb(btn_a, action_button_event_handler, LV_EVENT_RELEASED,
                        (void *)(uintptr_t)GAMEPAD_BTN_A);

    // Button B (right)
    lv_obj_t *btn_b = create_button(scr, "B",
                                     action_center_x + action_spacing,
                                     action_center_y - action_btn_size / 2,
                                     action_btn_size, action_btn_size,
                                     lv_color_hex(0xAA0000));
    lv_obj_add_event_cb(btn_b, action_button_event_handler, LV_EVENT_PRESSED,
                        (void *)(uintptr_t)GAMEPAD_BTN_B);
    lv_obj_add_event_cb(btn_b, action_button_event_handler, LV_EVENT_RELEASED,
                        (void *)(uintptr_t)GAMEPAD_BTN_B);

    // Button X (left)
    lv_obj_t *btn_x = create_button(scr, "X",
                                     action_center_x - action_btn_size - action_spacing,
                                     action_center_y - action_btn_size / 2,
                                     action_btn_size, action_btn_size,
                                     lv_color_hex(0x0000AA));
    lv_obj_add_event_cb(btn_x, action_button_event_handler, LV_EVENT_PRESSED,
                        (void *)(uintptr_t)GAMEPAD_BTN_X);
    lv_obj_add_event_cb(btn_x, action_button_event_handler, LV_EVENT_RELEASED,
                        (void *)(uintptr_t)GAMEPAD_BTN_X);

    // Button Y (top)
    lv_obj_t *btn_y = create_button(scr, "Y",
                                     action_center_x - action_btn_size / 2,
                                     action_center_y - action_btn_size - action_spacing,
                                     action_btn_size, action_btn_size,
                                     lv_color_hex(0xAAAA00));
    lv_obj_add_event_cb(btn_y, action_button_event_handler, LV_EVENT_PRESSED,
                        (void *)(uintptr_t)GAMEPAD_BTN_Y);
    lv_obj_add_event_cb(btn_y, action_button_event_handler, LV_EVENT_RELEASED,
                        (void *)(uintptr_t)GAMEPAD_BTN_Y);

    ESP_LOGI(TAG, "Gamepad UI initialized");
}
