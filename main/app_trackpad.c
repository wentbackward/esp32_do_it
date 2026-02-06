#include "app_trackpad.h"
#include "app_hid_trackpad.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_touch.h"

static const char *TAG = "app_trackpad";

// Runtime state
static esp_lcd_touch_handle_t s_touch = NULL;
static app_hid_t *s_hid = NULL;
static trackpad_state_t s_gesture_state;
static TaskHandle_t s_task_handle = NULL;

// Shared status for UI (atomic access assumed for 32-bit types)
static volatile int32_t s_status_x = 0;
static volatile int32_t s_status_y = 0;
static volatile bool s_status_touched = false;
static volatile trackpad_zone_t s_status_zone = TRACKPAD_ZONE_MAIN;

// Config
static uint16_t s_hres = 0;
static uint16_t s_vres = 0;
static int32_t s_scroll_w = 0;
static int32_t s_scroll_h = 0;

// Scroll state
static float s_scroll_accum_v = 0.0f;
static float s_scroll_accum_h = 0.0f;
static trackpad_zone_t s_touch_start_zone = TRACKPAD_ZONE_MAIN;

// ========================== Helper Functions ==========================

static uint32_t get_timestamp_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// ========================== Click Queue ==========================

static volatile uint8_t s_pending_clicks = 0;
static volatile uint8_t s_click_phase = 0; // 0=idle, 1=pressed, 2=released
static volatile uint32_t s_click_time = 0;

#define CLICK_PRESS_MS 10
#define CLICK_GAP_MS 30

static void queue_clicks(uint8_t count, uint32_t now)
{
    s_pending_clicks = count;
    s_click_phase = 0;
    s_click_time = now;
}

static void process_pending_clicks(uint32_t now)
{
    if (s_pending_clicks == 0) return;

    uint32_t elapsed = now - s_click_time;

    if (s_click_phase == 0) {
        app_hid_trackpad_send_click(s_hid, 0x01);
        s_click_phase = 1;
        s_click_time = now;
    } else if (s_click_phase == 1 && elapsed >= CLICK_PRESS_MS) {
        app_hid_trackpad_send_click(s_hid, 0x00);
        s_click_phase = 2;
        s_click_time = now;
        s_pending_clicks--;
    } else if (s_click_phase == 2 && s_pending_clicks > 0 && elapsed >= CLICK_GAP_MS) {
        s_click_phase = 0;
    } else if (s_click_phase == 2 && s_pending_clicks == 0) {
        s_click_phase = 0;
    }
}

// ========================== Action Executor ==========================

static void execute_action(const trackpad_action_t *action, uint32_t now)
{
    switch (action->type) {
        case TRACKPAD_ACTION_MOVE:
            app_hid_trackpad_send_move(s_hid, action->dx, action->dy);
            break;
        case TRACKPAD_ACTION_CLICK_DOWN:
            queue_clicks(1, now);
            break;
        case TRACKPAD_ACTION_DOUBLE_CLICK:
            queue_clicks(2, now);
            break;
        case TRACKPAD_ACTION_TRIPLE_CLICK:
            queue_clicks(3, now);
            break;
        case TRACKPAD_ACTION_QUADRUPLE_CLICK:
            queue_clicks(4, now);
            break;
        case TRACKPAD_ACTION_DRAG_START:
            app_hid_trackpad_send_click(s_hid, 0x01);
            break;
        case TRACKPAD_ACTION_DRAG_MOVE:
            app_hid_trackpad_send_report(s_hid, 0x01, action->dx, action->dy, 0, 0);
            break;
        case TRACKPAD_ACTION_DRAG_END:
            app_hid_trackpad_send_click(s_hid, 0x00);
            break;
        case TRACKPAD_ACTION_SCROLL_V:
            app_hid_trackpad_send_scroll(s_hid, action->scroll_v, 0);
            break;
        case TRACKPAD_ACTION_SCROLL_H:
            app_hid_trackpad_send_scroll(s_hid, 0, action->scroll_h);
            break;
        default:
            break;
    }
}

// ========================== Polling Task ==========================

static void trackpad_poll_task(void *arg)
{
    (void)arg;
    const TickType_t poll_interval = pdMS_TO_TICKS(10); // 100Hz

    // Wait for system stabilize
    vTaskDelay(pdMS_TO_TICKS(500));

    static int32_t last_x = 0;
    static int32_t last_y = 0;
    static bool was_touched = false;

    while (1) {
        if (!s_touch || !s_hid) {
            vTaskDelay(poll_interval);
            continue;
        }

        uint32_t now = get_timestamp_ms();

        // Hardware poll
        esp_lcd_touch_read_data(s_touch);
        
        uint16_t x = 0, y = 0, strength = 0;
        uint8_t point_num = 0;
        bool touched = esp_lcd_touch_get_coordinates(s_touch, &x, &y, &strength, &point_num, 1);

        // Apply 180-degree coordinate transformation
        // Touch panel is physically mounted upside down relative to display
        x = s_hres - 1 - x;
        y = s_vres - 1 - y;

        // Update shared state
        s_status_x = x;
        s_status_y = y;
        s_status_touched = touched;
        s_status_zone = trackpad_get_zone(x, y, s_hres, s_vres, s_scroll_w, s_scroll_h);

        // Handle scroll zones vs main trackpad area
        trackpad_action_t action;
        bool in_scroll_zone = (s_status_zone == TRACKPAD_ZONE_SCROLL_V ||
                                s_status_zone == TRACKPAD_ZONE_SCROLL_H ||
                                s_status_zone == TRACKPAD_ZONE_SCROLL_CORNER);

        if (touched && !was_touched) {
            // Touch started - record zone
            s_touch_start_zone = s_status_zone;
            s_scroll_accum_v = 0.0f;
            s_scroll_accum_h = 0.0f;
        } else if (touched && was_touched && (s_touch_start_zone != TRACKPAD_ZONE_MAIN)) {
            // Moving in scroll zone - generate scroll actions
            int32_t dx = x - last_x;
            int32_t dy = y - last_y;

            if (s_touch_start_zone == TRACKPAD_ZONE_SCROLL_V || s_touch_start_zone == TRACKPAD_ZONE_SCROLL_CORNER) {
                // Vertical scroll - accumulate and emit
                s_scroll_accum_v += (float)dy / 20.0f;  // Sensitivity factor
                int8_t scroll_units = (int8_t)s_scroll_accum_v;
                if (scroll_units != 0) {
                    action.type = TRACKPAD_ACTION_SCROLL_V;
                    action.scroll_v = -scroll_units;  // Invert for natural scrolling
                    action.scroll_h = 0;
                    execute_action(&action, now);
                    s_scroll_accum_v -= (float)scroll_units;
                }
            }

            if (s_touch_start_zone == TRACKPAD_ZONE_SCROLL_H || s_touch_start_zone == TRACKPAD_ZONE_SCROLL_CORNER) {
                // Horizontal scroll - accumulate and emit
                s_scroll_accum_h += (float)dx / 20.0f;  // Sensitivity factor
                int8_t scroll_units = (int8_t)s_scroll_accum_h;
                if (scroll_units != 0) {
                    action.type = TRACKPAD_ACTION_SCROLL_H;
                    action.scroll_v = 0;
                    action.scroll_h = scroll_units;
                    execute_action(&action, now);
                    s_scroll_accum_h -= (float)scroll_units;
                }
            }
        } else if (!touched && was_touched) {
            // Touch released - reset scroll state
            s_scroll_accum_v = 0.0f;
            s_scroll_accum_h = 0.0f;
        }

        // Only process gestures if started in main trackpad area
        if (s_touch_start_zone == TRACKPAD_ZONE_MAIN) {
            trackpad_input_t input;
            input.x = x;
            input.y = y;
            input.timestamp_ms = now;
            bool process = false;

            if (touched && !was_touched) {
                input.type = TRACKPAD_EVENT_PRESSED;
                process = true;
            } else if (touched && was_touched) {
                input.type = TRACKPAD_EVENT_PRESSING;
                process = true;
            } else if (!touched && was_touched) {
                input.type = TRACKPAD_EVENT_RELEASED;
                input.x = last_x; // Use last known pos for release
                input.y = last_y;
                process = true;
            }

            if (process) {
                if (trackpad_process_input(&s_gesture_state, &input, &action)) {
                    execute_action(&action, now);
                }
            }
        }

        // Time-based tick
        if (trackpad_tick(now, &action)) {
            execute_action(&action, now);
        }

        // Click queue
        process_pending_clicks(now);

        if (touched) {
            last_x = x;
            last_y = y;
        }
        was_touched = touched;

        vTaskDelay(poll_interval);
    }
}

// ========================== Public API ==========================

esp_err_t app_trackpad_init(const app_trackpad_cfg_t *cfg)
{
    if (!cfg || !cfg->touch || !cfg->hid) {
        return ESP_ERR_INVALID_ARG;
    }

    s_touch = cfg->touch;
    s_hid = cfg->hid;
    s_hres = cfg->hres;
    s_vres = cfg->vres;
    s_scroll_w = cfg->scroll_zone_w;
    s_scroll_h = cfg->scroll_zone_h;

    // Initialize gesture engine
    trackpad_state_init(&s_gesture_state, s_hres, s_vres, s_scroll_w, s_scroll_h);

    // Start task
    BaseType_t ret = xTaskCreate(trackpad_poll_task, "trackpad_poll", 4096, NULL, 10, &s_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Trackpad service started (100Hz, Prio 10)");
    return ESP_OK;
}

void app_trackpad_get_status(app_trackpad_status_t *status)
{
    if (status) {
        status->x = s_status_x;
        status->y = s_status_y;
        status->touched = s_status_touched;
        status->zone = s_status_zone;
    }
}

void app_trackpad_update_config(int32_t scroll_w, int32_t scroll_h)
{
    s_scroll_w = scroll_w;
    s_scroll_h = scroll_h;
    // Note: trackpad_state_init updates the internal gesture config too
    trackpad_state_init(&s_gesture_state, s_hres, s_vres, s_scroll_w, s_scroll_h);
}
