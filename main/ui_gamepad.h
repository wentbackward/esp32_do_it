/**
 * @file ui_gamepad.h
 * @brief Gamepad UI - D-pad and action buttons for USB gamepad
 */

#pragma once

#include "app_hid.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Gamepad UI configuration
 */
typedef struct {
    uint16_t hres;      // Horizontal resolution
    uint16_t vres;      // Vertical resolution
    app_hid_t *hid;     // HID handle
} gamepad_cfg_t;

/**
 * @brief Initialize gamepad UI
 *
 * @param cfg Configuration structure
 */
void ui_gamepad_init(const gamepad_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
