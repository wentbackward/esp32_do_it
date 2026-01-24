#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *title;
    int32_t hres;
    int32_t vres;

    // Optional hooks (set to NULL if not supported)
    bool (*set_invert)(void *ctx, bool on);
    bool (*cycle_orientation)(void *ctx);
    bool (*set_backlight)(uint8_t pct);

    void *ctx; // passed back to hooks
} hwtest_cfg_t;

// Call once from LVGL thread (wrap with lvgl_port_lock/unlock)
void ui_hwtest_init(const hwtest_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
