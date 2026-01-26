#include "app_display_lgfx.h"

#include "sdkconfig.h"

#include "esp_check.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

#include "lgfx_auto_config.hpp"

static const char *TAG = "app_display_lgfx";

// Global LGFX instance
static LGFX *s_lgfx = nullptr;

#ifdef CONFIG_APP_LCD_BL_PWM_ENABLE
static ledc_channel_t s_bl_ledc_channel = LEDC_CHANNEL_0;
static uint32_t s_bl_max_duty = 0;
#endif

// Stub panel handle for compatibility with HAL interface
// LovyanGFX doesn't use esp_lcd_panel_handle_t, but we need to provide
// something for the interface compatibility
static esp_lcd_panel_handle_t s_stub_panel = (esp_lcd_panel_handle_t)0x1;

bool app_display_set_invert(void *ctx, bool on)
{
    if (!s_lgfx) {
        ESP_LOGW(TAG, "LGFX not initialized");
        return false;
    }

    s_lgfx->invertDisplay(on);
    ESP_LOGI(TAG, "Display invert: %s", on ? "ON" : "OFF");
    return true;
}

bool app_display_cycle_orientation(void *ctx)
{
    if (!s_lgfx) {
        ESP_LOGW(TAG, "LGFX not initialized");
        return false;
    }

    // Cycle through rotations: 0 -> 1 -> 2 -> 3 -> 0
    uint8_t current_rotation = s_lgfx->getRotation();
    uint8_t next_rotation = (current_rotation + 1) % 4;
    s_lgfx->setRotation(next_rotation);

    ESP_LOGI(TAG, "Rotation changed: %d -> %d", current_rotation, next_rotation);
    return true;
}

extern "C" esp_err_t app_display_init(app_display_t *out)
{
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null out");

    // GPIO 38: Control signal (purpose unclear, but required for Elecrow board)
    gpio_config_t ctrl_cfg = {
        .pin_bit_mask = (1ULL << 38),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&ctrl_cfg), TAG, "gpio_config GPIO38");
    gpio_set_level((gpio_num_t)38, 0);  // LOW per Elecrow example
    ESP_LOGI(TAG, "GPIO 38 (control) set LOW");

    // Create LovyanGFX instance
    s_lgfx = new LGFX();
    if (!s_lgfx) {
        ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "Failed to create LGFX instance");
    }

    // Initialize the display
    if (!s_lgfx->init()) {
        delete s_lgfx;
        s_lgfx = nullptr;
        ESP_RETURN_ON_ERROR(ESP_FAIL, TAG, "LGFX init failed");
    }

    #if CONFIG_APP_LCD_SWAP_BYTES
        s_lgfx->setSwapBytes(true);
    #else
        s_lgfx->setSwapBytes(false);
    #endif

    // Set initial rotation and color depth
    s_lgfx->setRotation(0);
    s_lgfx->setColorDepth(CONFIG_APP_LCD_COLOR_DEPTH);



    ESP_LOGI(TAG, "LovyanGFX initialized: %dx%d", s_lgfx->width(), s_lgfx->height());    

#ifdef CONFIG_APP_LCD_INVERT_DEFAULT
    if (CONFIG_APP_LCD_INVERT_DEFAULT) {
        s_lgfx->invertDisplay(true);
        ESP_LOGI(TAG, "Display invert enabled by default");
    }
#endif

    // Backlight PWM setup
    if (CONFIG_APP_LCD_PIN_BL >= 0) {
#ifdef CONFIG_APP_LCD_BL_PWM_ENABLE
        // Configure LEDC timer
        ledc_timer_config_t ledc_timer = {
            .speed_mode       = LEDC_LOW_SPEED_MODE,
            .duty_resolution  = (ledc_timer_bit_t)CONFIG_APP_LCD_BL_PWM_RESOLUTION,
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

        app_display_set_backlight_percent(CONFIG_APP_LCD_BL_DEFAULT_DUTY);

        ESP_LOGI(TAG, "Backlight PWM: %dHz, %d-bit, duty=%"PRIu32"/%"PRIu32" (%d%%)",
                 CONFIG_APP_LCD_BL_PWM_FREQ_HZ, CONFIG_APP_LCD_BL_PWM_RESOLUTION,
                 initial_duty, s_bl_max_duty, CONFIG_APP_LCD_BL_DEFAULT_DUTY);
#else
        // Simple on/off mode
        gpio_config_t bk = {
            .pin_bit_mask = 1ULL << CONFIG_APP_LCD_PIN_BL,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&bk), TAG, "bk gpio_config");
        gpio_set_level(CONFIG_APP_LCD_PIN_BL, 1);

        ESP_LOGI(TAG, "Backlight: simple on/off (GPIO %d)", CONFIG_APP_LCD_PIN_BL);
#endif
    } else {
        ESP_LOGI(TAG, "No backlight GPIO configured");
    }

    // Return stub handles for HAL compatibility
    out->panel = s_stub_panel;
    out->io = nullptr;  // LGFX doesn't use separate IO layer

    ESP_LOGI(TAG, "LovyanGFX Display init OK (%dx%d)", s_lgfx->width(), s_lgfx->height());
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

// Accessor for LVGL integration
extern "C" LGFX* app_display_get_lgfx(void)
{
    return s_lgfx;
}

// Push pixels from LVGL to LovyanGFX
// This is called from the LVGL flush callback in app_lvgl.c
extern "C" void lgfx_push_pixels(void *lgfx, int x1, int y1, int x2, int y2, const uint8_t *data)
{
    if (!lgfx) return;

    LGFX *gfx = static_cast<LGFX*>(lgfx);

    // LovyanGFX expects width and height, not x2/y2
    int w = x2 - x1;
    int h = y2 - y1;

    // Push the pixel data to the display
    gfx->startWrite();
    gfx->setAddrWindow(x1, y1, w, h);
    gfx->writePixels((uint16_t*)data, w * h);
    gfx->endWrite();
}
