/**
 * @file app_hid_trackpad.c
 * @brief Trackpad mode HID implementation (USB Mouse)
 */

#include "app_hid_trackpad.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "device/usbd.h"

static const char *TAG = "app_hid_trackpad";

// ========================== USB Descriptors ==========================

// HID report descriptor for mouse
static const uint8_t s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE()
};

// Total length of configuration descriptor
// Config descriptor + 1 HID interface
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

// USB Configuration Descriptor for HID Mouse
static const uint8_t s_hid_configuration_descriptor[] = {
    // Configuration Descriptor
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, 0, 100),

    // HID Mouse Interface Descriptor
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_MOUSE, sizeof(s_hid_report_descriptor), 0x81, 8, 10),
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

// USB event callback for esp_tinyusb
static void usb_event_cb(tinyusb_event_t *event, void *arg)
{
    (void)arg;
    switch (event->id) {
        case TINYUSB_EVENT_ATTACHED:
            ESP_LOGI(TAG, "USB attached to host");
            break;
        case TINYUSB_EVENT_DETACHED:
            ESP_LOGW(TAG, "USB detached from host");
            break;
        default:
            break;
    }
}

esp_err_t app_hid_init(app_hid_t *hid)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing USB HID Trackpad (Mouse)");

    // TinyUSB configuration with custom HID descriptor and event callback
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(usb_event_cb);

    // Set our custom HID configuration descriptor
    tusb_cfg.descriptor.full_speed_config = s_hid_configuration_descriptor;
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = s_hid_configuration_descriptor;
#endif

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    ESP_LOGI(TAG, "USB HID Trackpad initialized");
    return ESP_OK;
}

// Track HID ready state for debug logging
static bool s_hid_was_ready = false;

esp_err_t app_hid_trackpad_send_move(app_hid_t *hid, int16_t dx, int16_t dy)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if USB HID is ready
    bool ready = tud_hid_ready();
    if (ready != s_hid_was_ready) {
        ESP_LOGI(TAG, "HID ready state changed: %s", ready ? "READY" : "NOT READY");
        s_hid_was_ready = ready;
    }

    if (!ready) {
        return ESP_ERR_NOT_FINISHED;
    }

    // Clamp deltas to int8_t range [-127, 127]
    int8_t dx_clamped = (dx > 127) ? 127 : (dx < -127) ? -127 : (int8_t)dx;
    int8_t dy_clamped = (dy > 127) ? 127 : (dy < -127) ? -127 : (int8_t)dy;

    // Send mouse report (buttons=0, x, y, scroll=0, pan=0)
    bool sent = tud_hid_mouse_report(0, 0, dx_clamped, dy_clamped, 0, 0);
    ESP_LOGD(TAG, "Mouse move (%d,%d) sent=%d", dx_clamped, dy_clamped, sent);

    return ESP_OK;
}

esp_err_t app_hid_trackpad_send_click(app_hid_t *hid, uint8_t buttons)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if USB HID is ready
    if (!tud_hid_ready()) {
        ESP_LOGW(TAG, "Click ignored - HID not ready");
        return ESP_ERR_NOT_FINISHED;
    }

    // Send mouse report with button state (no movement)
    bool sent = tud_hid_mouse_report(0, buttons, 0, 0, 0, 0);
    ESP_LOGI(TAG, "Mouse click (buttons=0x%02X) sent=%d", buttons, sent);

    return ESP_OK;
}
