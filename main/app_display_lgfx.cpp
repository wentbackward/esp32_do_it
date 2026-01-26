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

bool app_display_set_invert(void *ctx, bool on_off)
{
    if (!s_lgfx) {
        ESP_LOGW(TAG, "LGFX not initialized");
        return false;
    }

    s_lgfx->invertDisplay(on_off);
    ESP_LOGI(TAG, "Display invert: %s", on_off ? "ON" : "OFF");
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

#if CONFIG_APP_LGFX_PANEL_RGB
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
#endif

    // Create LovyanGFX instance
    s_lgfx = new LGFX();
    if (!s_lgfx) {
        ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "Failed to create LGFX instance");
    }

    #if CONFIG_APP_LCD_SWAP_BYTES
        s_lgfx->setSwapBytes(true);
    #else
        s_lgfx->setSwapBytes(false);
    #endif

    // Set initial rotation and color depth
    s_lgfx->setRotation(CONFIG_APP_LCD_ROTATION_DEFAULT);
    s_lgfx->setColorDepth(CONFIG_APP_LCD_COLOR_DEPTH);


    // Initialize the display
    if (!s_lgfx->init()) {
        delete s_lgfx;
        s_lgfx = nullptr;
        ESP_RETURN_ON_ERROR(ESP_FAIL, TAG, "LGFX init failed");
    }

    ESP_LOGI(TAG, "LovyanGFX initialized: %dx%d", s_lgfx->width(), s_lgfx->height());

    #if CONFIG_APP_LCD_INVERT_DEFAULT
        s_lgfx->invertDisplay(true);
        ESP_LOGI(TAG, "Display invert enabled by default");
    #else
        s_lgfx->invertDisplay(false);
        ESP_LOGI(TAG, "Display invert disabled");
    #endif

    // Backlight setup - must be after init()
    if (CONFIG_APP_LCD_PIN_BL >= 0) {
#ifdef CONFIG_APP_LCD_BL_PWM_ENABLE
        // LGFX handles PWM backlight via Light_PWM instance configured in lgfx_auto_config.hpp
        uint8_t brightness = CONFIG_APP_LCD_BL_DEFAULT_DUTY;
        s_lgfx->setBrightness(brightness);
        ESP_LOGI(TAG, "Backlight PWM: %d%% (LGFX managed)", brightness);
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
        gpio_set_level((gpio_num_t)CONFIG_APP_LCD_PIN_BL, 1);

        ESP_LOGI(TAG, "Backlight: simple on/off (GPIO %d)", CONFIG_APP_LCD_PIN_BL);
#endif
    } else {
        ESP_LOGI(TAG, "No backlight GPIO configured");
    }


    // ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_stub_panel, true), TAG, "disp_on");
    // Return stub handles for HAL compatibility
    out->panel = s_stub_panel;
    out->io = nullptr;  // LGFX doesn't use separate IO layer

    ESP_LOGI(TAG, "LovyanGFX Display init OK (%dx%d)", s_lgfx->width(), s_lgfx->height());
    return ESP_OK;
}

#ifdef CONFIG_APP_LCD_BL_PWM_ENABLE
bool app_display_set_backlight_percent(uint8_t percent)
{
    if (!s_lgfx) {
        ESP_LOGW(TAG, "LGFX not initialized");
        return false;
    }

    if (percent > 100) percent = 100;

    s_lgfx->setBrightness(percent);
    // uint32_t duty = (s_bl_max_duty * percent) / 100;
    // return app_display_set_backlight_duty(duty) == ESP_OK;
    return true;
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
