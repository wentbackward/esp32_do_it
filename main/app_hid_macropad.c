/**
 * @file app_hid_macropad.c
 * @brief Macropad mode HID implementation (USB Keyboard)
 */

#include "app_hid_macropad.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include <string.h>

static const char *TAG = "app_hid_macropad";
static const char *NVS_NAMESPACE = "macropad";

static nvs_handle_t s_nvs_handle = 0;

// TinyUSB HID report descriptor for keyboard
static const uint8_t s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

// Default key mappings (number keys 1-9, 0)
static const uint8_t s_default_keycodes[] = {
    HID_KEY_1, HID_KEY_2, HID_KEY_3, HID_KEY_4,
    HID_KEY_5, HID_KEY_6, HID_KEY_7, HID_KEY_8,
    HID_KEY_9, HID_KEY_0
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

    ESP_LOGI(TAG, "Initializing USB HID Macropad (Keyboard)");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Open NVS namespace
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    // TinyUSB configuration - use default config
    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    ESP_LOGI(TAG, "USB HID Macropad initialized");
    return ESP_OK;
}

esp_err_t app_hid_macropad_load_mapping(uint8_t button_idx, uint8_t *modifier, uint8_t *keycode)
{
    if (!modifier || !keycode) {
        return ESP_ERR_INVALID_ARG;
    }

    // Build NVS key name
    char key_name[16];
    snprintf(key_name, sizeof(key_name), "btn_%u", button_idx);

    // Try to read from NVS (format: high byte=modifier, low byte=keycode)
    uint16_t mapping = 0;
    esp_err_t ret = nvs_get_u16(s_nvs_handle, key_name, &mapping);

    if (ret == ESP_OK) {
        *modifier = (mapping >> 8) & 0xFF;
        *keycode = mapping & 0xFF;
        ESP_LOGI(TAG, "Loaded mapping for button %u: mod=0x%02X key=0x%02X",
                 button_idx, *modifier, *keycode);
    } else {
        // Use default mapping
        *modifier = HID_MOD_NONE;
        if (button_idx < sizeof(s_default_keycodes)) {
            *keycode = s_default_keycodes[button_idx];
        } else {
            *keycode = 0;  // No key
        }
        ESP_LOGI(TAG, "Using default mapping for button %u: key=0x%02X", button_idx, *keycode);
    }

    return ESP_OK;
}

esp_err_t app_hid_macropad_save_mapping(uint8_t button_idx, uint8_t modifier, uint8_t keycode)
{
    // Build NVS key name
    char key_name[16];
    snprintf(key_name, sizeof(key_name), "btn_%u", button_idx);

    // Pack modifier and keycode into uint16_t
    uint16_t mapping = ((uint16_t)modifier << 8) | keycode;

    // Write to NVS
    esp_err_t ret = nvs_set_u16(s_nvs_handle, key_name, mapping);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save mapping: %s", esp_err_to_name(ret));
        return ret;
    }

    // Commit changes
    ret = nvs_commit(s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Saved mapping for button %u: mod=0x%02X key=0x%02X",
             button_idx, modifier, keycode);
    return ESP_OK;
}

esp_err_t app_hid_macropad_send_key(app_hid_t *hid, uint8_t modifier, uint8_t keycode)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if USB HID is ready
    if (!tud_hid_ready()) {
        return ESP_ERR_NOT_FINISHED;
    }

    // Build keycode array (only one key at a time for simplicity)
    uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};

    // Send keyboard report
    tud_hid_keyboard_report(0, modifier, keycodes);

    return ESP_OK;
}

esp_err_t app_hid_macropad_release_all(app_hid_t *hid)
{
    if (!hid) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if USB HID is ready
    if (!tud_hid_ready()) {
        return ESP_ERR_NOT_FINISHED;
    }

    // Send empty keyboard report (no keys pressed)
    uint8_t keycodes[6] = {0, 0, 0, 0, 0, 0};
    tud_hid_keyboard_report(0, 0, keycodes);

    return ESP_OK;
}
