#include "app_display_rgb.h"

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_rgb.h"

static const char *TAG = "app_display";

#ifdef CONFIG_APP_LCD_BL_PWM_ENABLE
static ledc_channel_t s_bl_ledc_channel = LEDC_CHANNEL_0;
static uint32_t s_bl_max_duty = 0;
#endif

static esp_lcd_panel_handle_t s_panel = NULL;

bool app_display_set_invert(void *ctx, bool on)
{
    // RGB panels don't typically support invert command
    ESP_LOGW(TAG, "Invert not supported on RGB panels");
    return false;
}

bool app_display_cycle_orientation(void *ctx)
{
    // RGB panels don't typically support orientation changes via commands
    // Would need to be handled at LVGL level
    ESP_LOGW(TAG, "Orientation cycling not supported on RGB panels");
    return false;
}

esp_err_t app_display_init(app_display_t *out)
{
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null out");

    // RGB LCD panel configuration
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = CONFIG_APP_LCD_RGB_PCLK_HZ,
            .h_res = CONFIG_APP_LCD_HRES,
            .v_res = CONFIG_APP_LCD_VRES,
            .hsync_pulse_width = CONFIG_APP_LCD_RGB_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = CONFIG_APP_LCD_RGB_HSYNC_BACK_PORCH,
            .hsync_front_porch = CONFIG_APP_LCD_RGB_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = CONFIG_APP_LCD_RGB_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = CONFIG_APP_LCD_RGB_VSYNC_BACK_PORCH,
            .vsync_front_porch = CONFIG_APP_LCD_RGB_VSYNC_FRONT_PORCH,
            .flags = {
                .hsync_idle_low = (CONFIG_APP_LCD_RGB_HSYNC_POLARITY == 0),
                .vsync_idle_low = (CONFIG_APP_LCD_RGB_VSYNC_POLARITY == 0),
                .de_idle_high = (CONFIG_APP_LCD_RGB_DE_IDLE_HIGH == 1),
                .pclk_active_neg = (CONFIG_APP_LCD_RGB_PCLK_ACTIVE_NEG == 1),
                .pclk_idle_high = (CONFIG_APP_LCD_RGB_PCLK_IDLE_HIGH == 1),
            },
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,  // Use single framebuffer in PSRAM, rely on bounce buffer
#ifdef CONFIG_APP_LVGL_BUFF_DMA
        .bounce_buffer_size_px = CONFIG_APP_LCD_HRES * CONFIG_APP_LVGL_BUF_LINES,
#endif
        .sram_trans_align = 64,
        .psram_trans_align = 64,
        .hsync_gpio_num = CONFIG_APP_LCD_RGB_PIN_HSYNC,
        .vsync_gpio_num = CONFIG_APP_LCD_RGB_PIN_VSYNC,
        .de_gpio_num = CONFIG_APP_LCD_RGB_PIN_DE,
        .pclk_gpio_num = CONFIG_APP_LCD_RGB_PIN_PCLK,
        .disp_gpio_num = GPIO_NUM_NC,
        .data_gpio_nums = {
            CONFIG_APP_LCD_RGB_PIN_D0,
            CONFIG_APP_LCD_RGB_PIN_D1,
            CONFIG_APP_LCD_RGB_PIN_D2,
            CONFIG_APP_LCD_RGB_PIN_D3,
            CONFIG_APP_LCD_RGB_PIN_D4,
            CONFIG_APP_LCD_RGB_PIN_D5,
            CONFIG_APP_LCD_RGB_PIN_D6,
            CONFIG_APP_LCD_RGB_PIN_D7,
            CONFIG_APP_LCD_RGB_PIN_D8,
            CONFIG_APP_LCD_RGB_PIN_D9,
            CONFIG_APP_LCD_RGB_PIN_D10,
            CONFIG_APP_LCD_RGB_PIN_D11,
            CONFIG_APP_LCD_RGB_PIN_D12,
            CONFIG_APP_LCD_RGB_PIN_D13,
            CONFIG_APP_LCD_RGB_PIN_D14,
            CONFIG_APP_LCD_RGB_PIN_D15,
        },
        .flags = {
            .fb_in_psram = 1,
#ifdef CONFIG_APP_LVGL_BUFF_DMA
            .bb_invalidate_cache = 1,
#endif
        },
    };

    esp_lcd_panel_handle_t panel = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&panel_config, &panel), TAG, "new_rgb_panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "panel_reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "panel_init");

    s_panel = panel;

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
        gpio_config_t bk = {
            .pin_bit_mask = 1ULL << CONFIG_APP_LCD_PIN_BL,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&bk), TAG, "bk gpio_config");
        gpio_set_level(CONFIG_APP_LCD_PIN_BL, 1);

        ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "disp_on");
        ESP_LOGI(TAG, "Backlight: simple on/off (GPIO %d)", CONFIG_APP_LCD_PIN_BL);
#endif
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "disp_on");
        ESP_LOGI(TAG, "No backlight GPIO configured");
    }

    out->panel = panel;
    out->io = NULL;  // RGB panels don't use IO layer
    ESP_LOGI(TAG, "RGB Display init OK (%dx%d, PCLK=%d Hz)",
             CONFIG_APP_LCD_HRES, CONFIG_APP_LCD_VRES, CONFIG_APP_LCD_RGB_PCLK_HZ);
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
