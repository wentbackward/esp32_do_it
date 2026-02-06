#pragma once

#include "esp_lcd_touch.h"
#include "app_hid.h"
#include "trackpad_gesture.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Trackpad configuration
 */
typedef struct {
    uint16_t hres;
    uint16_t vres;
    esp_lcd_touch_handle_t touch;
    app_hid_t *hid;
    // Configuration for scroll zones (can be disabled by setting to 0)
    int32_t scroll_zone_w; 
    int32_t scroll_zone_h;
} app_trackpad_cfg_t;

/**
 * @brief Trackpad runtime status (for UI visualization)
 */
typedef struct {
    int32_t x;
    int32_t y;
    bool touched;
    trackpad_zone_t zone;
} app_trackpad_status_t;

/**
 * @brief Initialize and start the trackpad service task
 * 
 * @param cfg Configuration structure
 * @return ESP_OK on success
 */
esp_err_t app_trackpad_init(const app_trackpad_cfg_t *cfg);

/**
 * @brief Get the current status of the trackpad cursor
 * Safe to call from UI task (thread safe)
 * 
 * @param status Pointer to status structure to fill
 */
void app_trackpad_get_status(app_trackpad_status_t *status);

/**
 * @brief Update configuration (e.g. scroll zones) at runtime
 * 
 * @param scroll_w Scroll zone width
 * @param scroll_h Scroll zone height
 */
void app_trackpad_update_config(int32_t scroll_w, int32_t scroll_h);

#ifdef __cplusplus
}
#endif
