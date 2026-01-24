#include "esp_check.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"

// from the FT6x36 component
#include "esp_lcd_touch_ft6x36.h"

static const char *TAG = "touch";

esp_err_t init_touch_ft6x36(esp_lcd_touch_handle_t *out_tp,
                            esp_lcd_panel_io_handle_t *out_io)
{
    ESP_RETURN_ON_FALSE(out_tp && out_io, ESP_ERR_INVALID_ARG, TAG, "null out ptr");

    // 1) Init I2C master
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_TOUCH_SDA_GPIO,
        .scl_io_num = CONFIG_TOUCH_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CONFIG_TOUCH_I2C_CLOCK_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(CONFIG_TOUCH_I2C_PORT, &cfg), TAG, "i2c_param_config");
    ESP_RETURN_ON_ERROR(i2c_driver_install(CONFIG_TOUCH_I2C_PORT, cfg.mode, 0, 0, 0),
                        TAG, "i2c_driver_install");

    // 2) Create an esp_lcd I2C IO handle for the touch controller
    esp_lcd_panel_io_i2c_config_t io_conf = ESP_LCD_TOUCH_IO_I2C_FT6x36_CONFIG();
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)CONFIG_TOUCH_I2C_PORT, &io_conf, out_io),
        TAG, "new_panel_io_i2c"
    );

    // 3) Touch configuration (IMPORTANT: x_max/y_max match your screen)
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 240,
        .y_max = 320,
        .rst_gpio_num = (CONFIG_TOUCH_RST_GPIO >= 0) ? CONFIG_TOUCH_RST_GPIO : -1,
        .int_gpio_num = (CONFIG_TOUCH_INT_GPIO >= 0) ? CONFIG_TOUCH_INT_GPIO : -1,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    // 4) Instantiate FT6x36 (covers FT6336)
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_ft6x36(*out_io, &tp_cfg, out_tp),
                        TAG, "touch_new_i2c_ft6x36");

    ESP_LOGI(TAG, "FT6x36/FT6336 ready");
    return ESP_OK;
}
