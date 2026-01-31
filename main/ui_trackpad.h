/**
 * @file ui_trackpad.h
 * @brief Trackpad UI - Touch panel as USB mouse
 *
 * Features:
 * - Logarithmic acceleration for movement
 * - Tap to click with swipe cancellation
 * - Click and hold/drag support
 * - Scroll zones on edges
 * - Mode toggle button
 */

#pragma once

#include "app_hid.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mode switch callback type
 *
 * Called when user presses the mode switch button.
 * Application should switch to the alternate UI (e.g., macropad).
 */
typedef void (*ui_trackpad_mode_switch_cb_t)(void);

/**
 * @brief Trackpad UI configuration
 */
typedef struct {
    uint16_t hres;                          // Horizontal resolution
    uint16_t vres;                          // Vertical resolution
    app_hid_t *hid;                         // HID handle
    ui_trackpad_mode_switch_cb_t mode_switch_cb;  // Optional: callback for mode switch button
} trackpad_cfg_t;

/**
 * @brief Initialize trackpad UI
 *
 * Creates the trackpad interface with:
 * - Main trackpad area (movement)
 * - Right edge scroll zone (vertical scroll)
 * - Bottom edge scroll zone (horizontal scroll)
 * - Mode switch button (top-left, if callback provided)
 *
 * @param cfg Configuration structure
 */
void ui_trackpad_init(const trackpad_cfg_t *cfg);

/**
 * @brief Set or update the mode switch callback
 *
 * @param cb Callback function (NULL to disable)
 */
void ui_trackpad_set_mode_switch_cb(ui_trackpad_mode_switch_cb_t cb);

#ifdef __cplusplus
}
#endif
