#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run hardware display test (no LVGL)
 *
 * Tests the RGB panel directly by drawing color fills and patterns.
 * Sequence: Red -> Green -> Blue -> Color bars
 *
 * @param panel LCD panel handle
 * @param hres Horizontal resolution
 * @param vres Vertical resolution
 * @return ESP_OK on success
 */
esp_err_t hw_display_test_run(esp_lcd_panel_handle_t panel, int hres, int vres);

#ifdef __cplusplus
}
#endif
