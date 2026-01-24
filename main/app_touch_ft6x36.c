#include "app_touch_ft6x36.h"

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#include "esp_lcd_touch_ft6x36.h"

static const char *TAG = "app_touch";

static void i2c_scan(i2c_port_t port)
{
    ESP_LOGI(TAG, "Scanning I2C on port %d...", port);
    for (int addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
    }
}

static esp_err_t touch_manual_reset(void)
{
    if (CONFIG_APP_TOUCH_PIN_RST < 0) return ESP_OK;

    gpio_config_t r = {
        .pin_bit_mask = 1ULL << CONFIG_APP_TOUCH_PIN_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&r), TAG, "touch rst gpio_config");

    gpio_set_level(CONFIG_APP_TOUCH_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_TOUCH_RESET_PULSE_MS));
    gpio_set_level(CONFIG_APP_TOUCH_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_TOUCH_RESET_BOOT_MS));
    return ESP_OK;
}

esp_err_t app_touch_init(app_touch_t *out)
{
    
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null out");

    const i2c_port_t port =
#if CONFIG_APP_TOUCH_I2C_PORT == 1
        I2C_NUM_1;
#else
        I2C_NUM_0;
#endif

    // I2C init (legacy driver you’re using; good enough for now)
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_APP_TOUCH_PIN_SDA,
        .scl_io_num = CONFIG_APP_TOUCH_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CONFIG_APP_TOUCH_I2C_CLOCK_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(port, &cfg), TAG, "i2c_param_config");
    ESP_RETURN_ON_ERROR(i2c_driver_install(port, cfg.mode, 0, 0, 0), TAG, "i2c_driver_install");

    ESP_LOGI(TAG, "I2C ready (clk=%d Hz)", CONFIG_APP_TOUCH_I2C_CLOCK_HZ);
    i2c_scan(port);

    // Manual reset + boot delay (your known-good sequence)
    ESP_RETURN_ON_ERROR(touch_manual_reset(), TAG, "touch_manual_reset");
    ESP_LOGI(TAG, "Touch reset done");
    i2c_scan(port);

    // Touch IO
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t io_conf = ESP_LCD_TOUCH_IO_I2C_FT6x36_CONFIG();
    io_conf.dev_addr = CONFIG_APP_TOUCH_I2C_ADDR;

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)port, &io_conf, &tp_io),
        TAG, "new_panel_io_i2c"
    );

    // Touch driver config
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = CONFIG_APP_LCD_HRES,
        .y_max = CONFIG_APP_LCD_VRES,
        .rst_gpio_num = -1,                 // IMPORTANT: manual reset used
        .int_gpio_num = -1,                 // leave off (stable)
        .levels = { .reset = 1, .interrupt = 1 },
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };

    esp_lcd_touch_handle_t tp = NULL;
    ESP_RETURN_ON_ERROR(
        esp_lcd_touch_new_i2c_ft6x36(tp_io, &tp_cfg, &tp),
        TAG, "touch_new_i2c_ft6x36"
    );

    out->tp = tp;
    out->tp_io = tp_io;
    ESP_LOGI(TAG, "Touch init OK (addr=0x%02X)", CONFIG_APP_TOUCH_I2C_ADDR);
    return ESP_OK;
}
