/**
 * @file ui_macropad.h
 * @brief Macropad UI - Button grid for USB keyboard macros
 */

#pragma once

#include "app_hid.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macropad UI configuration
 */
typedef struct {
    uint16_t hres;          // Horizontal resolution
    uint16_t vres;          // Vertical resolution
    app_hid_t *hid;         // HID handle
    uint8_t button_rows;    // Number of button rows
    uint8_t button_cols;    // Number of button columns
} macropad_cfg_t;

/**
 * @brief Initialize macropad UI
 *
 * @param cfg Configuration structure
 */
void ui_macropad_init(const macropad_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
