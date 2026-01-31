/**
 * @file ui_trackpad.h
 * @brief Trackpad UI - Touch panel as USB mouse
 */

#pragma once

#include "app_hid.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Trackpad UI configuration
 */
typedef struct {
    uint16_t hres;      // Horizontal resolution
    uint16_t vres;      // Vertical resolution
    app_hid_t *hid;     // HID handle
} trackpad_cfg_t;

/**
 * @brief Initialize trackpad UI
 *
 * @param cfg Configuration structure
 */
void ui_trackpad_init(const trackpad_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
