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
#include "tusb_cdc_acm.h"
#include <stdarg.h>

static const char *TAG = "app_hid_trackpad";

// ========================== USB Descriptors ==========================

// HID report descriptor for mouse
static const uint8_t s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE()
};

// Total length of configuration descriptor
// Config descriptor + CDC (IAD + 2 interfaces) + HID (1 interface)
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

// Endpoints
#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT   0x02
#define EPNUM_CDC_IN    0x82
#define EPNUM_HID       0x83

// USB Configuration Descriptor for Composite Device (CDC + HID Mouse)
static const uint8_t s_hid_configuration_descriptor[] = {
    // Configuration Descriptor
    // Config number, interface count, string index, total length, attribute, power in mA
    // Interface count = 3 (CDC Control + CDC Data + HID)
    TUD_CONFIG_DESCRIPTOR(1, 3, 0, TUSB_DESC_TOTAL_LEN, 0, 100),

    // CDC Control Interface (0) & Data Interface (1)
    // Interface number, string index, EP notification address and size, EP data out address, EP data in address, EP data size
    TUD_CDC_DESCRIPTOR(0, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

    // HID Mouse Interface Descriptor (2)
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(2, 0, HID_ITF_PROTOCOL_MOUSE, sizeof(s_hid_report_descriptor), EPNUM_HID, 8, 1),
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

// Custom log handler to redirect ESP_LOGx to USB CDC
static int cdc_log_vprintf(const char *fmt, va_list args)
{
    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len > 0) {
        // Attempt to write regardless of connection state (DTR)
        // This ensures logs are sent even if the terminal doesn't assert DTR
        tud_cdc_write(buf, len);
        tud_cdc_write_flush();
    }
    return len;
}

esp_err_t app_hid_init(app_hid_t *hid)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing USB HID Trackpad (Mouse) + CDC Console");

    // TinyUSB configuration with custom HID descriptor and event callback
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(usb_event_cb);

    // Set our custom HID configuration descriptor
    tusb_cfg.descriptor.full_speed_config = s_hid_configuration_descriptor;
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = s_hid_configuration_descriptor;
#endif

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // Give the USB stack a moment to stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

    // Register custom log handler to send logs to USB CDC
    esp_log_set_vprintf(cdc_log_vprintf);

    // Direct write to confirm path
    const char *msg = "\r\n=== CDC LOGGING READY ===\r\n";
    tud_cdc_write(msg, strlen(msg));
    tud_cdc_write_flush();

    ESP_LOGI(TAG, "USB HID Trackpad initialized (CDC Console Active)");
    
    return ESP_OK;
}

// Track HID ready state for debug logging
static bool s_hid_was_ready = false;

esp_err_t app_hid_trackpad_send_move(app_hid_t *hid, int16_t dx, int16_t dy)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clamp deltas to int8_t range [-127, 127]
    int8_t dx_clamped = (dx > 127) ? 127 : (dx < -127) ? -127 : (int8_t)dx;
    int8_t dy_clamped = (dy > 127) ? 127 : (dy < -127) ? -127 : (int8_t)dy;

    // Retry loop to handle transient busy states
    for (int i = 0; i < 5; i++) {
        if (tud_hid_ready()) {
            tud_hid_mouse_report(0, 0, dx_clamped, dy_clamped, 0, 0);
            return ESP_OK;
        }
        // Wait 1ms before retrying
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // If still not ready after retries, log warning and fail
    ESP_LOGW(TAG, "Move ignored - HID busy");
    return ESP_ERR_NOT_FINISHED;
}

esp_err_t app_hid_trackpad_send_click(app_hid_t *hid, uint8_t buttons)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    // Retry loop
    for (int i = 0; i < 5; i++) {
        if (tud_hid_ready()) {
            tud_hid_mouse_report(0, buttons, 0, 0, 0, 0);
            ESP_LOGD(TAG, "Mouse click (buttons=0x%02X) sent", buttons);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGW(TAG, "Click ignored - HID busy");
    return ESP_ERR_NOT_FINISHED;
}

esp_err_t app_hid_trackpad_send_scroll(app_hid_t *hid, int8_t vertical, int8_t horizontal)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    // Retry loop
    for (int i = 0; i < 5; i++) {
        if (tud_hid_ready()) {
            tud_hid_mouse_report(0, 0, 0, 0, vertical, horizontal);
            ESP_LOGD(TAG, "Mouse scroll (v=%d, h=%d) sent", vertical, horizontal);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGW(TAG, "Scroll ignored - HID busy");
    return ESP_ERR_NOT_FINISHED;
}

esp_err_t app_hid_trackpad_send_report(app_hid_t *hid, uint8_t buttons,
                                        int16_t dx, int16_t dy,
                                        int8_t scroll_v, int8_t scroll_h)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clamp deltas to int8_t range [-127, 127]
    int8_t dx_clamped = (dx > 127) ? 127 : (dx < -127) ? -127 : (int8_t)dx;
    int8_t dy_clamped = (dy > 127) ? 127 : (dy < -127) ? -127 : (int8_t)dy;

    // Retry loop
    for (int i = 0; i < 5; i++) {
        if (tud_hid_ready()) {
            tud_hid_mouse_report(0, buttons, dx_clamped, dy_clamped, scroll_v, scroll_h);
            ESP_LOGD(TAG, "Mouse report (btn=0x%02X, dx=%d, dy=%d) sent", buttons, dx_clamped, dy_clamped);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGW(TAG, "Report ignored - HID busy");
    return ESP_ERR_NOT_FINISHED;
}
