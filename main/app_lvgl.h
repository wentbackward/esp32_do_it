#pragma once
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_disp_t *disp;
    lv_indev_t *indev;  // can be NULL if touch missing
} app_lvgl_handles_t;

esp_err_t app_lvgl_init_and_add(const esp_lcd_panel_handle_t panel,
                                const esp_lcd_panel_io_handle_t io,
                                esp_lcd_touch_handle_t tp_or_null,
                                app_lvgl_handles_t *out);

#ifdef __cplusplus
}
#endif
