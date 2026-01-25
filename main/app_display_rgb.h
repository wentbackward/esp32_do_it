#pragma once
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;  // NULL for RGB (no IO layer)
} app_display_t;

esp_err_t app_display_init(app_display_t *out);

/* Generic hooks for UI/tools */
bool app_display_set_invert(void *ctx, bool on);
bool app_display_cycle_orientation(void *ctx);

/* Backlight PWM control */
bool app_display_set_backlight_percent(uint8_t percent);
esp_err_t app_display_set_backlight_duty(uint32_t duty);
uint32_t app_display_get_backlight_duty(void);

#ifdef __cplusplus
}
#endif
