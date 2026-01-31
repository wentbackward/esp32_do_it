/**
 * @file app_hid.h
 * @brief Common HID interface for USB device modes
 *
 * Provides HAL abstraction for USB HID functionality with build-time
 * selection of trackpad, macropad, or gamepad modes.
 */

#pragma once

#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HID device handle (opaque)
 */
typedef struct {
    void *priv;  // Private data for mode-specific implementation
} app_hid_t;

/**
 * @brief Initialize USB HID device
 *
 * Initializes TinyUSB stack and configures HID mode based on build configuration.
 *
 * @param hid Pointer to HID handle structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_hid_init(app_hid_t *hid);

#if CONFIG_APP_HID_MODE_TRACKPAD
/**
 * @brief Send mouse movement delta
 *
 * @param hid HID handle
 * @param dx X delta (will be clamped to [-127, 127])
 * @param dy Y delta (will be clamped to [-127, 127])
 * @return ESP_OK on success, ESP_ERR_NOT_FINISHED if USB not ready
 */
esp_err_t app_hid_trackpad_send_move(app_hid_t *hid, int16_t dx, int16_t dy);

/**
 * @brief Send mouse button click
 *
 * @param hid HID handle
 * @param buttons Button bitmask (bit 0=left, bit 1=right, bit 2=middle)
 * @return ESP_OK on success, ESP_ERR_NOT_FINISHED if USB not ready
 */
esp_err_t app_hid_trackpad_send_click(app_hid_t *hid, uint8_t buttons);
#endif // CONFIG_APP_HID_MODE_TRACKPAD

#if CONFIG_APP_HID_MODE_MACROPAD
/**
 * @brief Send keyboard key press
 *
 * @param hid HID handle
 * @param modifier Modifier keys bitmask (CTRL, SHIFT, ALT, etc.)
 * @param keycode HID keycode
 * @return ESP_OK on success, ESP_ERR_NOT_FINISHED if USB not ready
 */
esp_err_t app_hid_macropad_send_key(app_hid_t *hid, uint8_t modifier, uint8_t keycode);

/**
 * @brief Release all keyboard keys
 *
 * @param hid HID handle
 * @return ESP_OK on success
 */
esp_err_t app_hid_macropad_release_all(app_hid_t *hid);

/**
 * @brief Load key mapping from NVS for button index
 *
 * @param button_idx Button index (0-based)
 * @param modifier Output: modifier byte
 * @param keycode Output: HID keycode
 * @return ESP_OK on success, uses defaults if key not found
 */
esp_err_t app_hid_macropad_load_mapping(uint8_t button_idx, uint8_t *modifier, uint8_t *keycode);

/**
 * @brief Save key mapping to NVS for button index
 *
 * @param button_idx Button index (0-based)
 * @param modifier Modifier byte
 * @param keycode HID keycode
 * @return ESP_OK on success
 */
esp_err_t app_hid_macropad_save_mapping(uint8_t button_idx, uint8_t modifier, uint8_t keycode);
#endif // CONFIG_APP_HID_MODE_MACROPAD

#if CONFIG_APP_HID_MODE_GAMEPAD
/**
 * @brief Gamepad state structure
 */
typedef struct {
    int8_t x;         // D-pad X axis (-1=left, 0=center, 1=right)
    int8_t y;         // D-pad Y axis (-1=up, 0=center, 1=down)
    uint8_t buttons;  // Button bitmask (bit 0=A, 1=B, 2=X, 3=Y, etc.)
} gamepad_state_t;

/**
 * @brief Send gamepad state
 *
 * @param hid HID handle
 * @param state Pointer to gamepad state structure
 * @return ESP_OK on success, ESP_ERR_NOT_FINISHED if USB not ready
 */
esp_err_t app_hid_gamepad_send_state(app_hid_t *hid, const gamepad_state_t *state);
#endif // CONFIG_APP_HID_MODE_GAMEPAD

#ifdef __cplusplus
}
#endif
