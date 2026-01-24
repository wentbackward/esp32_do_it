#pragma once
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_lcd_touch_handle_t tp;
    esp_lcd_panel_io_handle_t tp_io;
} app_touch_t;

esp_err_t app_touch_init(app_touch_t *out);

#ifdef __cplusplus
}
#endif
