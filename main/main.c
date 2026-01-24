#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

#include "app_display_ili9341.h"
#include "app_touch_ft6x36.h"
#include "app_lvgl.h"
#include "esp_lvgl_port.h"

#include "ui_hwtest.h"

#include "lvgl.h"
#include "demos/lv_demos.h" 

static const char *TAG = "app_main";

static void ui_simple_start(void)
{
    lv_obj_t *scr = lv_screen_active();

    lv_obj_t *bar_red = lv_obj_create(scr);
    lv_obj_set_size(bar_red, 240, 30);
    lv_obj_align(bar_red, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar_red, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_width(bar_red, 0, 0);

    lv_obj_t *bar_green = lv_obj_create(scr);
    lv_obj_set_size(bar_green, 240, 30);
    lv_obj_align(bar_green, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(bar_green, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(bar_green, 0, 0);

    lv_obj_t *bar_blue = lv_obj_create(scr);
    lv_obj_set_size(bar_blue, 240, 30);
    lv_obj_align(bar_blue, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(bar_blue, lv_color_hex(0x0000FF), 0);
    lv_obj_set_style_border_width(bar_blue, 0, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL Simple Test");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 160, 70);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "Tap me");
    lv_obj_center(lbl);
}

void app_main(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "%s cores=%d rev=%d flash=%" PRIu32 "MB",
             CONFIG_IDF_TARGET, chip_info.cores, chip_info.revision,
             flash_size / (uint32_t)(1024 * 1024));

    // Display
    app_display_t disp_hw = {0};
    ESP_ERROR_CHECK(app_display_init(&disp_hw));

    // Touch (optional)
    app_touch_t touch = {0};
    esp_lcd_touch_handle_t tp = NULL;
#if CONFIG_APP_TOUCH_ENABLE
    if (app_touch_init(&touch) == ESP_OK) tp = touch.tp;
    else ESP_LOGW(TAG, "Touch init failed; continuing without touch");
#endif

    // LVGL
    app_lvgl_handles_t lv = {0};
    ESP_ERROR_CHECK(app_lvgl_init_and_add(disp_hw.panel, disp_hw.io, tp, &lv));
    (void)lv;

    // UI selection
    hwtest_cfg_t hwcfg = {
        .title = "HW Test (generic)",
        .hres = CONFIG_APP_LCD_HRES,
        .vres = CONFIG_APP_LCD_VRES,
        .set_invert = app_display_set_invert,
        .cycle_orientation = app_display_cycle_orientation,
        .set_backlight = NULL,
        .ctx = (void*)disp_hw.io,
    };

    lvgl_port_lock(0);
#if CONFIG_APP_UI_SIMPLE
    ui_simple_start();
#elif CONFIG_APP_UI_DEMO
    #if LV_USE_DEMO_WIDGETS
        lv_demo_widgets();
    #elif LV_USE_DEMO_BENCHMARK
        lv_demo_benchmark();
    #elif LV_USE_DEMO_MUSIC
        lv_demo_music();
    #else
        lv_obj_t *l = lv_label_create(lv_screen_active());
        lv_label_set_text(l, "No LVGL demos enabled. Turn on LV_USE_DEMO_* in menuconfig.");
        lv_obj_center(l);
    #endif
#elif CONFIG_APP_UI_HWTEST
    ui_hwtest_init(&hwcfg);
#endif
    lvgl_port_unlock();

    ESP_LOGI(TAG, "Running.");
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
