#include "ui_hwtest.h"

#include <stdio.h>
#include <inttypes.h>

#include "esp_log.h"


static hwtest_cfg_t s_cfg;

static lv_obj_t *s_touch_dot;
static lv_obj_t *s_touch_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_grid_info_label;
static lv_obj_t *s_inv_btn_label;
static lv_obj_t *s_orient_btn_label;
static lv_obj_t *s_bl_slider_label;
static lv_obj_t *s_test_btn_label;

static lv_timer_t *s_anim_timer;
static lv_timer_t *s_fps_timer;
static lv_obj_t *s_anim_bar;

static uint32_t s_frames = 0;
static uint32_t s_last_frames = 0;

static bool s_invert = false;
static uint8_t s_bl_pct = 100;

static int s_bar_x = 0;
static int s_bar_dir = 1;

static uint32_t s_click_count = 0;

static const char *TAG = "ui_hwtest.c";

/* ---------------- Touch overlay ---------------- */
static void touch_layer_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSING && code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED) return;

    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (s_touch_dot) {
        lv_obj_set_pos(s_touch_dot, (lv_coord_t)(p.x - 6), (lv_coord_t)(p.y - 6));
    }

    if (s_touch_label) {
        char buf[80];
        snprintf(buf, sizeof(buf),
                 "Touch: x=%" PRId32 " y=%" PRId32 " (%s)",
                 p.x, p.y, (code == LV_EVENT_RELEASED) ? "up" : "down");
        lv_label_set_text(s_touch_label, buf);
    }
}

/* ---------------- FPS-ish indicator ---------------- */
static void fps_timer_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t now = s_frames;
    uint32_t fps = now - s_last_frames;
    s_last_frames = now;

    if (s_status_label) {
        char buf[120];
        snprintf(buf, sizeof(buf),
                 "%s | %" PRId32 "x%" PRId32 " | FPS-ish: %lu",
                 (s_cfg.title ? s_cfg.title : "HW Bring-up Toolkit"),
                 s_cfg.hres, s_cfg.vres,
                 (unsigned long)fps);
        lv_label_set_text(s_status_label, buf);
    }
}

/* ---------------- Motion stress ---------------- */
static void anim_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_anim_bar) return;

    lv_obj_t *scr = lv_screen_active();
    int W = (int)lv_obj_get_width(scr);

    s_bar_x += s_bar_dir * 6;
    if (s_bar_x < 0) { s_bar_x = 0; s_bar_dir = 1; }
    if (s_bar_x > W - 16) { s_bar_x = W - 16; s_bar_dir = -1; }

    lv_obj_set_x(s_anim_bar, (lv_coord_t)s_bar_x);
    s_frames++;
}

/* ---------------- Grid line helpers (LVGL 9 safe) ---------------- */
/* Use a single static array for all grid points to avoid fragmentation */
#define MAX_GRID_POINTS 400  // Supports up to 200 lines (each line = 2 points)
static lv_point_precise_t s_grid_points[MAX_GRID_POINTS];
static int s_point_idx = 0;

static lv_obj_t *mk_line(lv_obj_t *parent,
                         lv_point_precise_t p0, lv_point_precise_t p1,
                         uint32_t color_hex, lv_opa_t opa)
{
    if (s_point_idx + 2 > MAX_GRID_POINTS) {
        ESP_LOGW(TAG, "Grid point array full, skipping line");
        return NULL;
    }

    lv_obj_t *l = lv_line_create(parent);

    // Store points in static array
    s_grid_points[s_point_idx] = p0;
    s_grid_points[s_point_idx + 1] = p1;

    // Line references into the static array
    lv_line_set_points(l, &s_grid_points[s_point_idx], 2);
    s_point_idx += 2;

    lv_obj_set_style_line_width(l, 1, 0);
    lv_obj_set_style_line_color(l, lv_color_hex(color_hex), 0);
    lv_obj_set_style_line_opa(l, opa, 0);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_SCROLLABLE);
    return l;
}

static void grid_build(lv_obj_t *scr, int step_minor, int step_major)
{
    int32_t W = lv_obj_get_width(scr);
    int32_t H = lv_obj_get_height(scr);
    int maj_color = 0x000000;
    int min_color = 0xAAAAAA;

    s_point_idx = 0;  // Reset point index

    // Minor grid lines
    for (int32_t x = 0; x <= W; x += step_minor) {
        lv_obj_t *line = mk_line(scr,
                (lv_point_precise_t){ x, 0 },
                (lv_point_precise_t){ x, H },
                min_color, LV_OPA_50);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);

    }
    for (int32_t y = 0; y <= H; y += step_minor) {
        lv_obj_t *line = mk_line(scr,
                (lv_point_precise_t){ 0, y },
                (lv_point_precise_t){ W, y },
                min_color, LV_OPA_50);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    }

    // Major grid lines
    for (int32_t x = 0; x <= W; x += step_major) {
        lv_obj_t *line = mk_line(scr,
                (lv_point_precise_t){ x, 0 },
                (lv_point_precise_t){ x, H },
                maj_color, LV_OPA_100);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    }
    for (int32_t y = 0; y <= H; y += step_major) {
        lv_obj_t *line = mk_line(scr,
                (lv_point_precise_t){ 0, y },
                (lv_point_precise_t){ W, y },
                maj_color, LV_OPA_100);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    }

    ESP_LOGI(TAG, "Grid built: %d points used (max %d)", s_point_idx, MAX_GRID_POINTS);
}

/* ---------------- UI widgets ---------------- */
static lv_obj_t *mk_box(lv_obj_t *parent, int w, int h, uint32_t hex, const char *txt)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, (lv_coord_t)w, (lv_coord_t)h);
    lv_obj_set_style_bg_color(o, lv_color_hex(hex), 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0x404040), 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(o);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    return o;
}

/* ---------------- Controls (hooks) ---------------- */
static void invert_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (!s_cfg.set_invert) {
        if (s_inv_btn_label) lv_label_set_text(s_inv_btn_label, "Invert: n/a");
        return;
    }

    bool next = !s_invert;
    bool ok = s_cfg.set_invert(s_cfg.ctx, next);
    if (ok) s_invert = next;

    if (s_inv_btn_label) lv_label_set_text(s_inv_btn_label, s_invert ? "Invert: ON" : "Invert: OFF");
}

static void orient_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (!s_cfg.cycle_orientation) {
        if (s_orient_btn_label) lv_label_set_text(s_orient_btn_label, "Orient: n/a");
        return;
    }

    bool ok = s_cfg.cycle_orientation(s_cfg.ctx);
    if (s_orient_btn_label) lv_label_set_text(s_orient_btn_label, ok ? "Orient: cycled" : "Orient: failed");
}

static void bl_slider_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    lv_obj_t *slider = lv_event_get_target(e);
    int v = (int)lv_slider_get_value(slider);
    if (v < 0) v = 0;
    if (v > 100) v = 100;

    s_bl_pct = (uint8_t)v;

    if (s_bl_slider_label) {
        char buf[24];
        snprintf(buf, sizeof(buf), "BL: %u%%", (unsigned)s_bl_pct);
        lv_label_set_text(s_bl_slider_label, buf);
    }

    if (s_cfg.set_backlight) {
        (void)s_cfg.set_backlight(s_bl_pct);
    }
}

static void test_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    s_click_count++;

    if (s_test_btn_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Clicks: %lu", (unsigned long)s_click_count);
        lv_label_set_text(s_test_btn_label, buf);
    }

    ESP_LOGI(TAG, "Test button clicked: %lu", (unsigned long)s_click_count);
}

/* ---------------- Public init ---------------- */
void ui_hwtest_init(const hwtest_cfg_t *cfg)
{
    if (cfg) s_cfg = *cfg;

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xEEEEEE), 0);

    int W = (int)lv_obj_get_width(scr);
    int H = (int)lv_obj_get_height(scr);

    // Calculate proportional grid spacing that evenly divides screen dimensions
    // Goal: major grid divides both W and H evenly for clean appearance
    // Try common spacings (20, 40, 50, 80, 100) and pick best fit
    int grid_major = 50;  // default
    int best_score = 1000000;

    int candidates[] = {20, 40, 50, 80, 100};
    for (int i = 0; i < 5; i++) {
        int spacing = candidates[i];
        int lines_w = W / spacing;
        int lines_h = H / spacing;

        // Calculate how evenly it divides (0 = perfect)
        int remainder_w = W % spacing;
        int remainder_h = H % spacing;

        // Prefer even division, and reasonable number of lines (4-10)
        int score = remainder_w + remainder_h;
        if (lines_w < 4 || lines_h < 4 || lines_w > 10 || lines_h > 10) {
            score += 1000;  // Penalize if too few or too many lines
        }

        if (score <= best_score) {
            best_score = score;
            grid_major = spacing;  // On tie, prefer larger spacing (fewer lines)
        }
    }

    // Calculate minor grid spacing (1/4 of major for good visual density)
    int grid_minor = grid_major / 4;

    // Ensure we don't exceed point array capacity
    // Max lines = (W/minor + H/minor + W/major + H/major) * 2 points per line
    int estimated_points = ((W/grid_minor + H/grid_minor + W/grid_major + H/grid_major) * 2);
    if (estimated_points > MAX_GRID_POINTS) {
        // Scale up spacing if we'd exceed capacity
        int scale = (estimated_points / MAX_GRID_POINTS) + 1;
        grid_minor *= scale;
        grid_major *= scale;
        ESP_LOGW(TAG, "Grid scaled up by %dx to fit point array limit", scale);
    }

    // Calculate grid line counts for display
    int minor_lines_v = (W / grid_minor) + 1;  // Vertical lines (including edges)
    int minor_lines_h = (H / grid_minor) + 1;  // Horizontal lines
    int major_lines_v = (W / grid_major) + 1;
    int major_lines_h = (H / grid_major) + 1;
    int total_lines = minor_lines_v + minor_lines_h + major_lines_v + major_lines_h;

    ESP_LOGI(TAG, "Build gridlines: %dx%d screen, minor=%dpx, major=%dpx, total_lines=%d",
             W, H, grid_minor, grid_major, total_lines);
    grid_build(scr, grid_minor, grid_major);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, (s_cfg.title ? s_cfg.title : "HW Bring-up Toolkit (LVGL)"));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    // Status line (also shows FPS-ish)
    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, (s_cfg.title ? s_cfg.title : "HW Bring-up Toolkit"));
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 22);

    // Grid info (shows line count - each line is a UI element affecting FPS)
    s_grid_info_label = lv_label_create(scr);
    char grid_buf[64];
    snprintf(grid_buf, sizeof(grid_buf), "Grid: %d lines (Major:%dpx Minor:%dpx)",
             total_lines, grid_minor, grid_major);
    lv_label_set_text(s_grid_info_label, grid_buf);
    lv_obj_align(s_grid_info_label, LV_ALIGN_TOP_LEFT, 4, 40);

    // Corner markers
    ESP_LOGI(TAG, "Create the corner markers");
    uint32_t box_border = 4;
    lv_obj_t *box_tl = mk_box(scr, 44, 24, 0x202020, "TL"); lv_obj_align(box_tl, LV_ALIGN_TOP_LEFT, box_border, box_border);
    lv_obj_t *box_tr = mk_box(scr, 44, 24, 0x202020, "TR"); lv_obj_align(box_tr, LV_ALIGN_TOP_RIGHT, -box_border, box_border);
    lv_obj_t *box_bl = mk_box(scr, 44, 24, 0x202020, "BL"); lv_obj_align(box_bl, LV_ALIGN_BOTTOM_LEFT, box_border, -box_border);
    lv_obj_t *box_br = mk_box(scr, 44, 24, 0x202020, "BR"); lv_obj_align(box_br, LV_ALIGN_BOTTOM_RIGHT, -box_border, -box_border);

    ESP_LOGI(TAG, "Create the color swatches");
    lv_obj_t *sw = lv_obj_create(scr);
    lv_obj_set_size(sw, (lv_coord_t)(W - 8), (48 + 16));
    lv_obj_align(sw, LV_ALIGN_BOTTOM_MID, 0, -32);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(sw, 1, 0);
    lv_obj_set_style_border_color(sw, lv_color_hex(0x404040), 0);
    lv_obj_set_style_pad_all(sw, 4, 0);
    lv_obj_set_style_pad_gap(sw, 4, 0);
    lv_obj_set_flex_flow(sw, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_clear_flag(sw, LV_OBJ_FLAG_SCROLLABLE);

    int boxw = (W / 4) - 8;
    int boxh = 24;

    mk_box(sw, boxw, boxh, 0xFF0000, "R");
    mk_box(sw, boxw, boxh, 0x00FF00, "G");
    mk_box(sw, boxw, boxh, 0x0000FF, "B");
    mk_box(sw, boxw, boxh, 0xFFFFFF, "W");

    mk_box(sw, boxw, boxh, 0x00FFFF, "C");
    mk_box(sw, boxw, boxh, 0xFF00FF, "M");
    mk_box(sw, boxw, boxh, 0xFFFF00, "Y");
    mk_box(sw, boxw, boxh, 0x000000, "K");

    // Touch label (just above swatches)
    s_touch_label = lv_label_create(scr);
    lv_label_set_text(s_touch_label, "Touch: x=? y=?");
    lv_obj_align(s_touch_label, LV_ALIGN_BOTTOM_LEFT, 6, -100);

    ESP_LOGI(TAG, "Create the touch dot");
    s_touch_dot = lv_obj_create(scr);
    lv_obj_set_size(s_touch_dot, 12, 12);
    lv_obj_set_style_radius(s_touch_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_touch_dot, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(s_touch_dot, 2, 0);
    lv_obj_set_style_border_color(s_touch_dot, lv_color_hex(0xFF0000), 0);
    lv_obj_clear_flag(s_touch_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_touch_dot, (lv_coord_t)(W / 2), (lv_coord_t)(H / 2));

    ESP_LOGI(TAG, "Full-screen transparent touch receiver");
    lv_obj_t *touch_layer = lv_obj_create(scr);
    lv_obj_set_size(touch_layer, (lv_coord_t)W, (lv_coord_t)H);
    lv_obj_align(touch_layer, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(touch_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(touch_layer, 0, 0);
    lv_obj_clear_flag(touch_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(touch_layer, touch_layer_cb, LV_EVENT_ALL, NULL);

    ESP_LOGI(TAG, "Moving translucent bar (tearing/flicker)");
    s_anim_bar = lv_obj_create(scr);
    lv_obj_set_size(s_anim_bar, 16, (lv_coord_t)(H - 16));
    lv_obj_align(s_anim_bar, LV_ALIGN_TOP_LEFT, 0, 8);
    lv_obj_set_style_bg_color(s_anim_bar, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_bg_opa(s_anim_bar, LV_OPA_30, 0);
    lv_obj_set_style_border_width(s_anim_bar, 0, 0);
    lv_obj_clear_flag(s_anim_bar, LV_OBJ_FLAG_SCROLLABLE);
    s_bar_x = 0;
    s_bar_dir = 1;

    // Controls (top-right) - only create if hardware supports them
    int y_pos = 52;

    // Invert button (only for displays that support it)
    if (s_cfg.set_invert) {
        ESP_LOGI(TAG, "Add Invert button");
        lv_obj_t *btn_inv = lv_button_create(scr);
        lv_obj_set_size(btn_inv, 120, 32);
        lv_obj_align(btn_inv, LV_ALIGN_TOP_RIGHT, -6, y_pos);
        lv_obj_add_event_cb(btn_inv, invert_btn_cb, LV_EVENT_CLICKED, NULL);
        s_inv_btn_label = lv_label_create(btn_inv);
        lv_label_set_text(s_inv_btn_label, "Invert: toggle");
        lv_obj_center(s_inv_btn_label);
        y_pos += 36;
    }

    // Orientation button (only for displays that support it)
    if (s_cfg.cycle_orientation) {
        ESP_LOGI(TAG, "Add cycle orientation button");
        lv_obj_t *btn_or = lv_button_create(scr);
        lv_obj_set_size(btn_or, 120, 32);
        lv_obj_align(btn_or, LV_ALIGN_TOP_RIGHT, -6, y_pos);
        lv_obj_add_event_cb(btn_or, orient_btn_cb, LV_EVENT_CLICKED, NULL);
        s_orient_btn_label = lv_label_create(btn_or);
        lv_label_set_text(s_orient_btn_label, "Orient: cycle");
        lv_obj_center(s_orient_btn_label);
        y_pos += 36;
    }

    // Backlight slider (only if hardware supports it)
    if (s_cfg.set_backlight) {
        ESP_LOGI(TAG, "Add backlight (brightness) slider");
        y_pos += 8; // Extra spacing before slider
        lv_obj_t *bl_slider = lv_slider_create(scr);
        lv_obj_set_size(bl_slider, 120, 12);
        lv_obj_align(bl_slider, LV_ALIGN_TOP_RIGHT, -12, y_pos);
        lv_slider_set_range(bl_slider, 0, 100);
        lv_slider_set_value(bl_slider, 100, LV_ANIM_ON);
        lv_obj_add_event_cb(bl_slider, bl_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

        s_bl_slider_label = lv_label_create(scr);
        lv_label_set_text(s_bl_slider_label, "BL: 100%");
        lv_obj_align(s_bl_slider_label, LV_ALIGN_TOP_RIGHT, -6, y_pos + 14);
    }

    // Test button (for click testing)
    ESP_LOGI(TAG, "Create test button");
    lv_obj_t *test_btn = lv_button_create(scr);
    lv_obj_set_size(test_btn, 160, 60);
    lv_obj_align(test_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(test_btn, test_btn_cb, LV_EVENT_CLICKED, NULL);

    s_test_btn_label = lv_label_create(test_btn);
    lv_label_set_text(s_test_btn_label, "Tap to Test");
    lv_obj_center(s_test_btn_label);



    // Timers
    ESP_LOGI(TAG, "Set Timers for animation");
    s_anim_timer = lv_timer_create(anim_timer_cb, 30, NULL);
    s_fps_timer  = lv_timer_create(fps_timer_cb, 1000, NULL);
    ESP_LOGI(TAG, "Init complete");
}
