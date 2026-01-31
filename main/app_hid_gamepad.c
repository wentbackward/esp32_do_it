/**
 * @file app_hid_gamepad.c
 * @brief Gamepad mode HID implementation (USB Gamepad)
 */

#include "app_hid_gamepad.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"

static const char *TAG = "app_hid_gamepad";

// TinyUSB HID report descriptor for gamepad
static const uint8_t s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_GAMEPAD()
};

// TinyUSB callbacks
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

esp_err_t app_hid_init(app_hid_t *hid)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing USB HID Gamepad");

    // TinyUSB configuration - use default config
    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    ESP_LOGI(TAG, "USB HID Gamepad initialized");
    return ESP_OK;
}

esp_err_t app_hid_gamepad_send_state(app_hid_t *hid, const gamepad_state_t *state)
{
    if (!hid || !state) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if USB HID is ready
    if (!tud_hid_ready()) {
        return ESP_ERR_NOT_FINISHED;
    }

    // Map D-pad coordinates to axis values (center=127, range 0-254)
    // X: -1=0, 0=127, 1=254
    // Y: -1=0, 0=127, 1=254
    uint8_t axis_x = (state->x == -1) ? 0 : (state->x == 1) ? 254 : 127;
    uint8_t axis_y = (state->y == -1) ? 0 : (state->y == 1) ? 254 : 127;

    // Build gamepad report
    // Report ID=0, X, Y, Z=0, Rz=0, Rx=0, Ry=0, hat=0, buttons
    hid_gamepad_report_t report = {
        .x = axis_x,
        .y = axis_y,
        .z = 127,
        .rz = 127,
        .rx = 127,
        .ry = 127,
        .hat = 0,
        .buttons = state->buttons
    };

    // Send gamepad report
    tud_hid_report(0, &report, sizeof(report));

    return ESP_OK;
}
