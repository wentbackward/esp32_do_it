#include "app_lvgl.h"

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "app_lvgl";

esp_err_t app_lvgl_init_and_add(const esp_lcd_panel_handle_t panel,
                                const esp_lcd_panel_io_handle_t io,
                                esp_lcd_touch_handle_t tp_or_null,
                                app_lvgl_handles_t *out)
{
    ESP_RETURN_ON_FALSE(panel && io && out, ESP_ERR_INVALID_ARG, TAG, "bad args");

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl_port_init");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = CONFIG_APP_LCD_HRES * 60,
        .double_buffer = true,
        .hres = CONFIG_APP_LCD_HRES,
        .vres = CONFIG_APP_LCD_VRES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy  = (CONFIG_APP_ROT_SWAP_XY != 0),
            .mirror_x = (CONFIG_APP_ROT_MIRROR_X != 0),
            .mirror_y = (CONFIG_APP_ROT_MIRROR_Y != 0),
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = CONFIG_APP_LCD_SWAP_BYTES,
        },
    };

    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    ESP_RETURN_ON_FALSE(disp, ESP_FAIL, TAG, "lvgl_port_add_disp failed");

    lv_indev_t *indev = NULL;
    if (tp_or_null) {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = disp,
            .handle = tp_or_null,
        };
        indev = lvgl_port_add_touch(&touch_cfg);
    }

    out->disp = disp;
    out->indev = indev;

    ESP_LOGI(TAG, "LVGL ready (swap_bytes=%d mirror_x=%d)", CONFIG_APP_LCD_SWAP_BYTES, CONFIG_APP_ROT_MIRROR_X);
    return ESP_OK;
}
