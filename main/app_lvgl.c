#include "app_lvgl.h"

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_heap_caps.h"

static const char *TAG = "app_lvgl";

// Flush callback for RGB panels
static void rgb_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2 + 1;
    int y2 = area->y2 + 1;
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, px_map);
    lv_display_flush_ready(disp);
}

esp_err_t app_lvgl_init_and_add(const esp_lcd_panel_handle_t panel,
                                const esp_lcd_panel_io_handle_t io,
                                esp_lcd_touch_handle_t tp_or_null,
                                app_lvgl_handles_t *out)
{
    ESP_RETURN_ON_FALSE(panel && out, ESP_ERR_INVALID_ARG, TAG, "bad args");

    // Initialize LVGL port (task and timer management)
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl_port_init");

    lv_disp_t *disp = NULL;

    if (io == NULL) {
        // RGB panel: use raw LVGL API
        lvgl_port_lock(0);

        lv_display_t *lv_disp = lv_display_create(CONFIG_APP_LCD_HRES, CONFIG_APP_LCD_VRES);
        if (!lv_disp) {
            lvgl_port_unlock();
            ESP_RETURN_ON_ERROR(ESP_FAIL, TAG, "lv_display_create failed");
        }

        // Store panel handle for flush callback
        lv_display_set_user_data(lv_disp, panel);

        // Set color format
        lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565);

        // Allocate buffers in PSRAM (RGB565 = 2 bytes/pixel)
        size_t buf_size = CONFIG_APP_LCD_HRES * CONFIG_APP_LVGL_BUF_LINES * 2;
        void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
        if (!buf1) {
            lvgl_port_unlock();
            ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "Buffer 1 alloc failed");
        }

        void *buf2 = NULL;
#ifdef CONFIG_APP_LVGL_DOUBLE_BUFFER
        buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
        if (!buf2) {
            free(buf1);
            lvgl_port_unlock();
            ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "Buffer 2 alloc failed");
        }
#endif

        lv_display_set_buffers(lv_disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(lv_disp, rgb_flush_cb);

        // Make this the default display
        lv_display_set_default(lv_disp);

        // Verify the active screen exists
        lv_obj_t *scr = lv_display_get_screen_active(lv_disp);
        ESP_LOGI(TAG, "Active screen: %p", scr);
        if (scr) {
            ESP_LOGI(TAG, "Screen class: %p", lv_obj_get_class(scr));
        } else {
            lvgl_port_unlock();
            ESP_RETURN_ON_ERROR(ESP_FAIL, TAG, "No active screen after display create");
        }

        lvgl_port_unlock();

        disp = lv_disp;
        ESP_LOGI(TAG, "LVGL ready (RGB, %d KB%s)", (int)(buf_size/1024), buf2 ? " x2" : "");
    } else {
        // SPI panel: use esp_lvgl_port
        lvgl_port_display_cfg_t disp_cfg = {
            .io_handle = io,
            .panel_handle = panel,
            .buffer_size = CONFIG_APP_LCD_HRES * CONFIG_APP_LVGL_BUF_LINES,
            .double_buffer = CONFIG_APP_LVGL_DOUBLE_BUFFER,
            .hres = CONFIG_APP_LCD_HRES,
            .vres = CONFIG_APP_LCD_VRES,
            .monochrome = false,
            .color_format = LV_COLOR_FORMAT_RGB565,
            .rotation = {
                .swap_xy  = (CONFIG_APP_ROT_SWAP_XY != 0),
                .mirror_x = (CONFIG_APP_ROT_MIRROR_X != 0),
                .mirror_y = (CONFIG_APP_ROT_MIRROR_Y != 0),
            },
            .flags = {
#ifdef CONFIG_APP_LVGL_BUFF_DMA
                .buff_dma = CONFIG_APP_LVGL_BUFF_DMA,
#else
                .buff_dma = false,
#endif
#ifdef CONFIG_APP_LCD_SWAP_BYTES
                .swap_bytes = CONFIG_APP_LCD_SWAP_BYTES,
#else
                .swap_bytes = false,
#endif
            },
        };

        disp = lvgl_port_add_disp(&disp_cfg);
        ESP_RETURN_ON_FALSE(disp, ESP_FAIL, TAG, "lvgl_port_add_disp failed");
        ESP_LOGI(TAG, "LVGL ready (SPI)");
    }

    // Add touch (works for both)
    lv_indev_t *indev = NULL;
    if (tp_or_null) {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = disp,
            .handle = tp_or_null,
        };
        indev = lvgl_port_add_touch(&touch_cfg);
    }

    out->disp = disp;
    out->indev = indev;

    return ESP_OK;
}
