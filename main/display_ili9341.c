#include "esp_check.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "driver/gpio.h"

static const char *TAG = "display";

esp_err_t init_display_ili9341(esp_lcd_panel_handle_t *out_panel,
                              esp_lcd_panel_io_handle_t *out_io)
{
    ESP_RETURN_ON_FALSE(out_panel && out_io, ESP_ERR_INVALID_ARG, TAG, "null out ptr");

    spi_bus_config_t buscfg = {
        .sclk_io_num = CONFIG_LCD_SCLK_GPIO,
        .mosi_io_num = CONFIG_LCD_MOSI_GPIO,
        .miso_io_num = (CONFIG_LCD_MISO_GPIO >= 0) ? CONFIG_LCD_MISO_GPIO : -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 320 * 60 * 2 + 8, // partial buffer strip (60 lines)
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(CONFIG_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO),
                        TAG, "spi_bus_initialize");

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = CONFIG_LCD_DC_GPIO,
        .cs_gpio_num = CONFIG_LCD_CS_GPIO,
        .pclk_hz = CONFIG_LCD_SPI_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)CONFIG_LCD_HOST, &io_cfg, out_io),
        TAG, "new_panel_io_spi"
    );

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = (CONFIG_LCD_RST_GPIO >= 0) ? CONFIG_LCD_RST_GPIO : -1,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(*out_io, &panel_cfg, out_panel),
                        TAG, "new_panel_ili9341");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*out_panel), TAG, "panel_reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*out_panel), TAG, "panel_init");

    // Start neutral; adjust once you see orientation on your hardware
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(*out_panel, false), TAG, "invert");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(*out_panel, false, false), TAG, "mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(*out_panel, false), TAG, "swap_xy");

    if (CONFIG_LCD_BKLT_GPIO >= 0) {
        gpio_config_t bk = {.pin_bit_mask = 1ULL << CONFIG_LCD_BKLT_GPIO, .mode = GPIO_MODE_OUTPUT};
        ESP_RETURN_ON_ERROR(gpio_config(&bk), TAG, "bk gpio");
        gpio_set_level(CONFIG_LCD_BKLT_GPIO, 1);
    }

    ESP_LOGI(TAG, "ILI9341 ready");
    return ESP_OK;
}
