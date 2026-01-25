#include "hw_display_test.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hw_display_test";

esp_err_t hw_display_test_run(esp_lcd_panel_handle_t panel, int hres, int vres)
{
    ESP_LOGI(TAG, "=== RGB Panel Hardware Test ===");
    ESP_LOGI(TAG, "Panel: %p, Resolution: %dx%d", panel, hres, vres);

    // Allocate a buffer for 10 lines at a time (reduce memory usage)
    size_t buf_size = hres * 10 * sizeof(uint16_t);
    uint16_t *buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate test buffer (%zu bytes)", buf_size);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Allocated %zu byte buffer at %p", buf_size, buf);

    // Test 1: Fill screen with RED
    ESP_LOGI(TAG, "Test 1/4: Fill screen RED");
    for (int i = 0; i < hres * 10; i++) {
        buf[i] = 0xF800; // RGB565 red
    }
    for (int y = 0; y < vres; y += 10) {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, 0, y, hres, y + 10, buf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "draw_bitmap failed at y=%d: %s", y, esp_err_to_name(ret));
            heap_caps_free(buf);
            return ret;
        }
    }
    ESP_LOGI(TAG, "RED fill complete");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Test 2: Fill screen with GREEN
    ESP_LOGI(TAG, "Test 2/4: Fill screen GREEN");
    for (int i = 0; i < hres * 10; i++) {
        buf[i] = 0x07E0; // RGB565 green
    }
    for (int y = 0; y < vres; y += 10) {
        esp_lcd_panel_draw_bitmap(panel, 0, y, hres, y + 10, buf);
    }
    ESP_LOGI(TAG, "GREEN fill complete");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Test 3: Fill screen with BLUE
    ESP_LOGI(TAG, "Test 3/4: Fill screen BLUE");
    for (int i = 0; i < hres * 10; i++) {
        buf[i] = 0x001F; // RGB565 blue
    }
    for (int y = 0; y < vres; y += 10) {
        esp_lcd_panel_draw_bitmap(panel, 0, y, hres, y + 10, buf);
    }
    ESP_LOGI(TAG, "BLUE fill complete");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Test 4: Color bars
    ESP_LOGI(TAG, "Test 4/4: 8 color bars");
    uint16_t colors[] = {
        0xF800, // Red
        0x07E0, // Green
        0x001F, // Blue
        0xFFE0, // Yellow
        0xF81F, // Magenta
        0x07FF, // Cyan
        0xFFFF, // White
        0x0000  // Black
    };
    const char *color_names[] = {
        "Red", "Green", "Blue", "Yellow", "Magenta", "Cyan", "White", "Black"
    };

    int bar_height = vres / 8;
    for (int bar = 0; bar < 8; bar++) {
        // Fill buffer with this color
        for (int i = 0; i < hres * 10; i++) {
            buf[i] = colors[bar];
        }

        // Draw the bar
        int y_start = bar * bar_height;
        int y_end = (bar == 7) ? vres : ((bar + 1) * bar_height); // Last bar fills to end
        for (int y = y_start; y < y_end; y += 10) {
            int y_actual_end = (y + 10 > y_end) ? y_end : (y + 10);
            esp_lcd_panel_draw_bitmap(panel, 0, y, hres, y_actual_end, buf);
        }
        ESP_LOGI(TAG, "  Bar %d/8: %s", bar + 1, color_names[bar]);
    }

    heap_caps_free(buf);
    ESP_LOGI(TAG, "=== Hardware test complete ===");
    ESP_LOGI(TAG, "Display should show 8 color bars from top to bottom");
    ESP_LOGI(TAG, "If you see this, RGB panel hardware is working correctly!");

    return ESP_OK;
}
