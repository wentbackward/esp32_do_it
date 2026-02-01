#include "app_touch_ft6x36.h"

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "esp_lcd_touch_ft6x36.h"

static const char *TAG = "app_touch";

// FT6X36 register addresses for tuning
#define FT6X36_REG_TH_GROUP     0x80  // Touch threshold (default 0x10)
#define FT6X36_REG_TH_DIFF      0x85  // Filter function coefficient
#define FT6X36_REG_CTRL         0x86  // Control register
#define FT6X36_REG_TIMEENTERMON 0x87  // Time to enter monitor mode

static void i2c_scan(i2c_master_bus_handle_t bus_handle)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        esp_err_t ret = i2c_master_probe(bus_handle, addr, 50);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
        }
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

    // I2C master bus init
    i2c_master_bus_handle_t i2c_handle = NULL;
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = CONFIG_APP_TOUCH_I2C_PORT,
        .sda_io_num = CONFIG_APP_TOUCH_PIN_SDA,
        .scl_io_num = CONFIG_APP_TOUCH_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &i2c_handle), TAG, "i2c_new_master_bus");

    ESP_LOGI(TAG, "I2C master bus ready");
    i2c_scan(i2c_handle);

    // Manual reset + boot delay (critical timing sequence)
    ESP_RETURN_ON_ERROR(touch_manual_reset(), TAG, "touch_manual_reset");
    ESP_LOGI(TAG, "Touch reset done");
    i2c_scan(i2c_handle);

    // Touch IO
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t io_conf = ESP_LCD_TOUCH_IO_I2C_FT6x36_CONFIG();
    io_conf.dev_addr = CONFIG_APP_TOUCH_I2C_ADDR;
    io_conf.scl_speed_hz = CONFIG_APP_TOUCH_I2C_CLOCK_HZ;

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c(i2c_handle, &io_conf, &tp_io),
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
    out->i2c_bus = i2c_handle;

    // Tune FT6X36 for smoother trackpad-like response
    // Higher threshold = less sensitive but better noise rejection
    uint8_t th_group = 0x50;   // Touch threshold (default 0x10, higher = less sensitive)
    uint8_t th_diff = 0x00;    // No filtering (causes deadzone on direction change)

    esp_lcd_panel_io_tx_param(tp_io, FT6X36_REG_TH_GROUP, &th_group, 1);
    esp_lcd_panel_io_tx_param(tp_io, FT6X36_REG_TH_DIFF, &th_diff, 1);

    ESP_LOGI(TAG, "FT6x36 touch init OK (addr=0x%02X), tuned for trackpad", CONFIG_APP_TOUCH_I2C_ADDR);
    return ESP_OK;
}
