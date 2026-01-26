#pragma once
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_lcd_touch_handle_t tp;
    esp_lcd_panel_io_handle_t tp_io;
    i2c_master_bus_handle_t i2c_bus;
} app_touch_t;

esp_err_t app_touch_init(app_touch_t *out);

#ifdef __cplusplus
}
#endif
