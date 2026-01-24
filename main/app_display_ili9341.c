#include "app_display_ili9341.h"

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"

static const char *TAG = "app_display";

#ifdef CONFIG_APP_LCD_BL_PWM_ENABLE
static ledc_channel_t s_bl_ledc_channel = LEDC_CHANNEL_0;
static uint32_t s_bl_max_duty = 0;
#endif

static inline spi_host_device_t host_from_kconfig(void)
{
#if CONFIG_APP_LCD_SPI_HOST == 3
    return SPI3_HOST;
#else
    return SPI2_HOST;
#endif
}

bool app_display_set_invert(void *ctx, bool on)
{
    esp_lcd_panel_io_handle_t io = (esp_lcd_panel_io_handle_t)ctx;
    // ILI9341: 0x21 invert ON, 0x20 invert OFF
    esp_err_t err = esp_lcd_panel_io_tx_param(io, on ? 0x21 : 0x20, NULL, 0);
    if (err == ESP_OK) ESP_LOGI(TAG, "Invert %s", on ? "ON" : "OFF");
    else ESP_LOGW(TAG, "Invert cmd failed: %s", esp_err_to_name(err));
    return err == ESP_OK;
}

bool app_display_cycle_orientation(void *ctx)
{
    esp_lcd_panel_io_handle_t io = (esp_lcd_panel_io_handle_t)ctx;

    // MADCTL presets (generic ILI9341-ish)
    static const uint8_t tbl[] = { 0x08, 0x48, 0x88, 0xC8, 0x28, 0x68, 0xA8, 0xE8 };
    static int i = 0;
    uint8_t madctl = tbl[i++ % (int)(sizeof(tbl)/sizeof(tbl[0]))];

    esp_err_t err = esp_lcd_panel_io_tx_param(io, 0x36, &madctl, 1);
    if (err == ESP_OK) ESP_LOGI(TAG, "MADCTL=0x%02X", madctl);
    else ESP_LOGW(TAG, "MADCTL write failed: %s", esp_err_to_name(err));
    return err == ESP_OK;
}

esp_err_t app_display_init(app_display_t *out)
{
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null out");

    const spi_host_device_t host = host_from_kconfig();

    // SPI bus config
    const spi_bus_config_t bus_config = ILI9341_PANEL_BUS_SPI_CONFIG(
        CONFIG_APP_LCD_PIN_SCK,
        CONFIG_APP_LCD_PIN_MOSI,
        CONFIG_APP_LCD_HRES * CONFIG_APP_LVGL_BUF_LINES * sizeof(uint16_t)
    );
    ESP_RETURN_ON_ERROR(spi_bus_initialize(host, &bus_config, SPI_DMA_CH_AUTO), TAG, "spi_bus_initialize");

    // IO config
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = ILI9341_PANEL_IO_SPI_CONFIG(
        CONFIG_APP_LCD_PIN_CS,
        CONFIG_APP_LCD_PIN_DC,
        NULL,
        NULL
    );
    io_config.pclk_hz = CONFIG_APP_LCD_SPI_CLOCK_HZ;

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)host, &io_config, &io),
        TAG, "new_panel_io_spi"
    );

    // Panel config
    esp_lcd_panel_handle_t panel = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = CONFIG_APP_LCD_PIN_RST,
#if CONFIG_APP_LCD_BGR
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
#else
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
#endif
        .bits_per_pixel = 16,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(io, &panel_config, &panel), TAG, "new_panel_ili9341");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "panel_reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "panel_init");

#ifdef CONFIG_APP_LCD_INVERT_DEFAULT
    app_display_set_invert(io, true);
#endif

    // Backlight PWM setup
    if (CONFIG_APP_LCD_PIN_BL >= 0) {
#ifdef CONFIG_APP_LCD_BL_PWM_ENABLE
        // Configure LEDC timer
        ledc_timer_config_t ledc_timer = {
            .speed_mode       = LEDC_LOW_SPEED_MODE,
            .duty_resolution  = CONFIG_APP_LCD_BL_PWM_RESOLUTION,
            .timer_num        = LEDC_TIMER_0,
            .freq_hz          = CONFIG_APP_LCD_BL_PWM_FREQ_HZ,
            .clk_cfg          = LEDC_AUTO_CLK
        };
        ESP_RETURN_ON_ERROR(ledc_timer_config(&ledc_timer), TAG, "ledc_timer_config");

        // Calculate max duty and initial brightness
        s_bl_max_duty = (1 << CONFIG_APP_LCD_BL_PWM_RESOLUTION) - 1;
        uint32_t initial_duty = (s_bl_max_duty * CONFIG_APP_LCD_BL_DEFAULT_DUTY) / 100;

        // Configure LEDC channel with initial duty
        ledc_channel_config_t ledc_channel = {
            .gpio_num   = CONFIG_APP_LCD_PIN_BL,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = s_bl_ledc_channel,
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = initial_duty,
            .hpoint     = 0
        };
        ESP_RETURN_ON_ERROR(ledc_channel_config(&ledc_channel), TAG, "ledc_channel_config");

        ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "disp_on");
        app_display_set_backlight_percent(CONFIG_APP_LCD_BL_DEFAULT_DUTY);

        ESP_LOGI(TAG, "Backlight PWM: %dHz, %d-bit, duty=%"PRIu32"/%"PRIu32" (%d%%)",
                 CONFIG_APP_LCD_BL_PWM_FREQ_HZ, CONFIG_APP_LCD_BL_PWM_RESOLUTION,
                 initial_duty, s_bl_max_duty, CONFIG_APP_LCD_BL_DEFAULT_DUTY);
#else
        // Simple on/off mode
        // gpio_config_t bk = {
        //     .pin_bit_mask = 1ULL << CONFIG_APP_LCD_PIN_BL,
        //     .mode = GPIO_MODE_OUTPUT,
        // };
        // ESP_RETURN_ON_ERROR(gpio_config(&bk), TAG, "bk gpio_config");
        // gpio_set_level(CONFIG_APP_LCD_PIN_BL, 1);

        ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "disp_on");
        ESP_LOGI(TAG, "Backlight: simple on/off (GPIO %d)", CONFIG_APP_LCD_PIN_BL);
#endif
    }

    out->panel = panel;
    out->io = io;
    ESP_LOGI(TAG, "Display init OK (%dx%d, SPI=%d Hz)", CONFIG_APP_LCD_HRES, CONFIG_APP_LCD_VRES, CONFIG_APP_LCD_SPI_CLOCK_HZ);
    return ESP_OK;
}

#ifdef CONFIG_APP_LCD_BL_PWM_ENABLE
bool app_display_set_backlight_percent(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = (s_bl_max_duty * percent) / 100;
    return app_display_set_backlight_duty(duty) == ESP_OK;
}

esp_err_t app_display_set_backlight_duty(uint32_t duty)
{
    if (duty > s_bl_max_duty) duty = s_bl_max_duty;

    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, s_bl_ledc_channel, duty), TAG, "ledc_set_duty");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, s_bl_ledc_channel), TAG, "ledc_update_duty");
    ESP_LOGI(TAG, "Backlight duty: %"PRIu32"/%"PRIu32" (%d%%)", duty, s_bl_max_duty, (int)((duty * 100) / s_bl_max_duty));
    return ESP_OK;
}

uint32_t app_display_get_backlight_duty(void)
{
    return ledc_get_duty(LEDC_LOW_SPEED_MODE, s_bl_ledc_channel);
}

#else

bool app_display_set_backlight_percent(uint8_t percent)
{
    ESP_LOGI(TAG, "PWM backlight not enabled");
    return false;
}

esp_err_t app_display_set_backlight_duty(uint32_t duty)
{
    ESP_LOGI(TAG, "PWM backlight not enabled");
    return ESP_ERR_NOT_SUPPORTED;
}

uint32_t app_display_get_backlight_duty(void)
{
    return 0;
}
#endif
