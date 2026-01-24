/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_check.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#include "lvgl.h"
#ifdef LV_BUILD_DEMO
#include "demo/lv_demo.h"
#endif

#include "esp_lvgl_port.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#include "esp_lcd_ili9341.h"

#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft6x36.h"   // from cfscn/esp_lcd_touch_ft6x36

#include "ui_hwtest.h"

static const char *TAG = "lvgl_demo";

/* ===== Your pin mapping (hardcoded for now) ===== */
#define LCD_HOST_ID          SPI2_HOST

#define LCD_RS_DC_GPIO       46   // LCD_RS = D/C
#define LCD_CS_GPIO          10
#define LCD_SCK_GPIO         12
#define LCD_MOSI_GPIO        11   // LCD_SDA = MOSI
#define LCD_MISO_GPIO        13   // LCD_SDO = MISO
#define LCD_RST_GPIO         -1   // tied to ESP reset
#define LCD_BL_GPIO          45

#define TOUCH_I2C_PORT       I2C_NUM_0
#define TOUCH_SCL_GPIO       15
#define TOUCH_SDA_GPIO       16
#define TOUCH_RST_GPIO       18
#define TOUCH_INT_GPIO       -1

/* Speeds */
#define LCD_SPI_CLK_HZ       (26 * 1000 * 1000)   // start conservative; later try 40MHz
#define TOUCH_I2C_CLK_HZ     100000
/* Display resolution */
#define LCD_HRES             240
#define LCD_VRES             320

#define UI_DEMO
#undef UI_HWTEST
#undef UI_SIMPLE

// Local functions
static void i2c_scan(i2c_port_t port);

// ILI9341-style adapter (example). For other panels, swap these hooks.
static bool hook_set_invert(void *ctx, bool on)
{
    esp_lcd_panel_io_handle_t io = (esp_lcd_panel_io_handle_t)ctx;
    ESP_LOGI(TAG, "Set Invert Display: %s", on ? "ON" : "OFF");
    return esp_lcd_panel_io_tx_param(io, on ? 0x21 : 0x20, NULL, 0) == ESP_OK;
}

static bool hook_cycle_orientation(void *ctx)
{
    esp_lcd_panel_io_handle_t io = (esp_lcd_panel_io_handle_t)ctx;
    static const uint8_t tbl[] = { 0x08, 0x48, 0x88, 0xC8, 0x28, 0x68, 0xA8, 0xE8 };
    static int i = 0;
    uint8_t madctl = tbl[i++ % (int)(sizeof(tbl)/sizeof(tbl[0]))];
    ESP_LOGI(TAG, "Cycle Orientation: MADCTL=0x%02X", madctl);
    return esp_lcd_panel_io_tx_param(io, 0x36, &madctl, 1) == ESP_OK;
}

/* ---------- Display init (ILI9341 over SPI) ---------- */
static esp_err_t init_display(esp_lcd_panel_handle_t *out_panel, esp_lcd_panel_io_handle_t *out_io)
{
    ESP_RETURN_ON_FALSE(out_panel && out_io, ESP_ERR_INVALID_ARG, TAG, "null out ptr");

    // SPI bus (use component macro; it sets sane max_transfer_sz)
    const spi_bus_config_t bus_config =
        ILI9341_PANEL_BUS_SPI_CONFIG(LCD_SCK_GPIO, LCD_MOSI_GPIO, LCD_HRES * 80 * sizeof(uint16_t));
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST_ID, &bus_config, SPI_DMA_CH_AUTO),
                        TAG, "spi_bus_initialize");

    // Panel IO (use component macro; it sets required defaults)
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config =
        ILI9341_PANEL_IO_SPI_CONFIG(LCD_CS_GPIO, LCD_RS_DC_GPIO, NULL, NULL);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST_ID, &io_config, &io_handle),
                        TAG, "new_panel_io_spi");

    // Panel driver config: NOTE rgb_ele_order + bits_per_pixel
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_GPIO,                 // -1 if tied to board reset
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,    // try BGR if colors look wrong
        .bits_per_pixel = 16,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle),
                        TAG, "new_panel_ili9341");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel_handle), TAG, "panel_reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel_handle), TAG, "panel_init");

    hook_set_invert(io_handle, true);

    // IMPORTANT: actually turn display on
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel_handle, true), TAG, "disp_on");

    // Backlight on (your code is fine here)
    if (LCD_BL_GPIO >= 0) {
        gpio_config_t bk = { .pin_bit_mask = 1ULL << LCD_BL_GPIO, .mode = GPIO_MODE_OUTPUT };
        ESP_RETURN_ON_ERROR(gpio_config(&bk), TAG, "bk gpio_config");
        gpio_set_level(LCD_BL_GPIO, 1);
    }

    *out_io = io_handle;
    *out_panel = panel_handle;
    return ESP_OK;
}

/* ---------- Touch init (FT6336 via FT6x36 driver over I2C) ---------- */
static esp_err_t init_touch(esp_lcd_touch_handle_t *out_tp, esp_lcd_panel_io_handle_t *out_tp_io)
{
    ESP_RETURN_ON_FALSE(out_tp && out_tp_io, ESP_ERR_INVALID_ARG, TAG, "null out ptr");

    // 1) I2C init
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TOUCH_SDA_GPIO,
        .scl_io_num = TOUCH_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = TOUCH_I2C_CLK_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(TOUCH_I2C_PORT, &i2c_cfg), TAG, "i2c_param_config");
    ESP_RETURN_ON_ERROR(i2c_driver_install(TOUCH_I2C_PORT, i2c_cfg.mode, 0, 0, 0),
                        TAG, "i2c_driver_install");

    ESP_LOGI(TAG, "I2C initialized for touch controller");
    i2c_scan(TOUCH_I2C_PORT);

    // 2) Manual reset pulse + boot delay (critical for many FT6336 boards)
    if (TOUCH_RST_GPIO >= 0) {
        gpio_config_t r = {
            .pin_bit_mask = 1ULL << TOUCH_RST_GPIO,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&r), TAG, "touch rst gpio_config");

        gpio_set_level(TOUCH_RST_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(TOUCH_RST_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(250));   // give controller time to boot
    }
    ESP_LOGI(TAG, "Touch controller reset complete");
    i2c_scan(TOUCH_I2C_PORT);

    // 3) IO handle for touch controller (force address 0x38)
    esp_lcd_panel_io_i2c_config_t io_conf = ESP_LCD_TOUCH_IO_I2C_FT6x36_CONFIG();
    io_conf.dev_addr = 0x38;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)TOUCH_I2C_PORT, &io_conf, out_tp_io),
        TAG, "new_panel_io_i2c"
    );

    ESP_LOGI(TAG, "Touch panel IO initialized");

    // 4) Touch driver config
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_HRES,
        .y_max = LCD_VRES,
        .rst_gpio_num = -1,  // We controlled reset manually above
        .int_gpio_num = TOUCH_INT_GPIO,
        .levels = { .reset = 1, .interrupt = 1 },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_ft6x36(*out_tp_io, &tp_cfg, out_tp),
                        TAG, "touch_new_i2c_ft6x36");

    ESP_LOGI(TAG, "Touch controller initialized");
    return ESP_OK;
}

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
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
        }
    }
}

static void btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        static int count = 0;
        count++;

        char buf[64];
        snprintf(buf, sizeof(buf), "Clicked %d", count);

        lv_obj_t *label = lv_obj_get_child(btn, 0);
        lv_label_set_text(label, buf);

        ESP_LOGI("UI", "Button clicked %d times", count);
    }
}

#ifdef UI_SIMPLE
static void ui_init(void)
{
// Expected	If wrong
// Red bar = red	If it’s blue → RGB/BGR swapped
// Green bar = green	If it’s purple → byte swap issue
// Blue bar = blue	If it’s red → RGB/BGR swapped

    lv_obj_t *scr = lv_screen_active();

    /* --------- Color test bars --------- */
    lv_obj_t *bar_red = lv_obj_create(scr);
    lv_obj_set_size(bar_red, 240, 30);
    lv_obj_align(bar_red, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar_red, lv_color_hex(0xFF0000), 0); // RED
    lv_obj_set_style_border_width(bar_red, 0, 0);

    lv_obj_t *bar_green = lv_obj_create(scr);
    lv_obj_set_size(bar_green, 240, 30);
    lv_obj_align(bar_green, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(bar_green, lv_color_hex(0x00FF00), 0); // GREEN
    lv_obj_set_style_border_width(bar_green, 0, 0);

    lv_obj_t *bar_blue = lv_obj_create(scr);
    lv_obj_set_size(bar_blue, 240, 30);
    lv_obj_align(bar_blue, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(bar_blue, lv_color_hex(0x0000FF), 0); // BLUE
    lv_obj_set_style_border_width(bar_blue, 0, 0);

    /* --------- Orientation labels --------- */
    lv_obj_t *top_label = lv_label_create(scr);
    lv_label_set_text(top_label, "TOP");
    lv_obj_align(top_label, LV_ALIGN_TOP_LEFT, 5, 5);

    lv_obj_t *bottom_label = lv_label_create(scr);
    lv_label_set_text(bottom_label, "BOTTOM");
    lv_obj_align(bottom_label, LV_ALIGN_BOTTOM_LEFT, 5, -5);

    lv_obj_t *left_label = lv_label_create(scr);
    lv_label_set_text(left_label, "LEFT");
    lv_obj_align(left_label, LV_ALIGN_LEFT_MID, 5, 0);

    lv_obj_t *right_label = lv_label_create(scr);
    lv_label_set_text(right_label, "RIGHT");
    lv_obj_align(right_label, LV_ALIGN_RIGHT_MID, -5, 0);

    /* --------- Main title --------- */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL 9.4 Color + Touch Test");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);

    /* --------- Button with callback --------- */
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 160, 70);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Tap me");
    lv_obj_center(btn_label);
}
#endif // UI_SIMPLE

void app_main(void)
{
    // Optional: keep your chip info print (no reboot loop)
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "%s, cores=%d, rev=%d, flash=%" PRIu32 "MB",
             CONFIG_IDF_TARGET, chip_info.cores, chip_info.revision,
             flash_size / (uint32_t)(1024 * 1024));

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t lcd_io = NULL;
    ESP_ERROR_CHECK(init_display(&panel, &lcd_io));

    esp_lcd_touch_handle_t tp = NULL;
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_err_t terr = init_touch(&tp, &tp_io);
    if (terr != ESP_OK) {
        ESP_LOGE(TAG, "Touch init failed (%s). Continuing WITHOUT touch.", esp_err_to_name(terr));
        tp = NULL;
    }

    // LVGL port init (creates tick + lv_timer handler task + lock)
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    // Add display to LVGL (LVGL 9 wants explicit color_format)
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = panel,
        .buffer_size = LCD_HRES * 60,      // pixels
        .double_buffer = true,
        .hres = LCD_HRES,
        .vres = LCD_VRES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };

    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    assert(disp);

    // Add touch (port bridges esp_lcd_touch -> LVGL indev)
    lv_indev_t *indev = NULL;
    if (tp) {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = disp,
            .handle = tp,
        };
        indev = lvgl_port_add_touch(&touch_cfg);
    }
    (void)indev;

    hwtest_cfg_t cfg = {
        .title = "HW Test (generic)",
        .hres = LCD_HRES,
        .vres = LCD_VRES,
        .set_invert = hook_set_invert,
        .cycle_orientation = hook_cycle_orientation,
        .set_backlight = NULL, // add later if you PWM BL
        .ctx = (void*)lcd_io,
    };

    lvgl_port_lock(0);
#ifdef UI_SIMPLE
    ui_init();
#elif defined(UI_DEMO)
    #ifdef LV_BUILD_DEMO
    lv_demo_widgets();
    #else
    ESP_LOGW(TAG, "LVGL demo not included in build");
    #endif
#elif defined(UI_HWTEST)
    ui_hwtest_init(&cfg);
#endif
    lvgl_port_unlock();
    ESP_LOGI(TAG, "Done. No restart loop; UI should stay running.");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
