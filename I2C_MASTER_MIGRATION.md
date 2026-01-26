# I2C Master API Migration - Completed

## Summary

Successfully migrated both touch drivers (`app_touch_gt911.c` and `app_touch_ft6x36.c`) from ESP-IDF's deprecated legacy I2C driver to the new I2C master driver API.

## Files Modified

### Header Files
1. **main/app_touch_gt911.h**
   - Added `#include "driver/i2c_master.h"`
   - Added `i2c_master_bus_handle_t i2c_bus` field to `app_touch_t` structure

2. **main/app_touch_ft6x36.h**
   - Added `#include "driver/i2c_master.h"`
   - Added `i2c_master_bus_handle_t i2c_bus` field to `app_touch_t` structure

### Implementation Files
3. **main/app_touch_gt911.c**
   - Changed include from `driver/i2c.h` to `driver/i2c_master.h`
   - Replaced `i2c_scan()` implementation with `i2c_master_probe()` API
   - Replaced legacy I2C init with new `i2c_master_bus_config_t` and `i2c_new_master_bus()`
   - Removed deprecated port casting - now passes handle directly
   - Added I2C clock speed to panel IO config (`io_conf.scl_speed_hz`)
   - Stores I2C bus handle in output structure

4. **main/app_touch_ft6x36.c**
   - **FIXED**: Changed wrong include from `driver/i2c_slave.h` to `driver/i2c_master.h`
   - Replaced `i2c_scan()` implementation with `i2c_master_probe()` API
   - Replaced legacy I2C init with new `i2c_master_bus_config_t` and `i2c_new_master_bus()`
   - Removed deprecated port casting - now passes handle directly
   - Added I2C clock speed to panel IO config (`io_conf.scl_speed_hz`)
   - **PRESERVED**: Critical manual reset timing sequence (unchanged)
   - Stores I2C bus handle in output structure

## Key Changes Summary

### Before (Legacy I2C API)
```c
#include "driver/i2c.h"

i2c_config_t cfg = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = CONFIG_APP_TOUCH_PIN_SDA,
    .scl_io_num = CONFIG_APP_TOUCH_PIN_SCL,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = CONFIG_APP_TOUCH_I2C_CLOCK_HZ,
};
i2c_param_config(port, &cfg);
i2c_driver_install(port, cfg.mode, 0, 0, 0);

// Scan with cmd_link
i2c_cmd_handle_t cmd = i2c_cmd_link_create();
i2c_master_start(cmd);
i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
i2c_master_stop(cmd);
esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(50));
i2c_cmd_link_delete(cmd);

// Cast port to handle (type mismatch!)
esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)port, &io_conf, &tp_io);
```

### After (New I2C Master API)
```c
#include "driver/i2c_master.h"

i2c_master_bus_handle_t i2c_handle = NULL;
const i2c_master_bus_config_t bus_config = {
    .i2c_port = CONFIG_APP_TOUCH_I2C_PORT,
    .sda_io_num = CONFIG_APP_TOUCH_PIN_SDA,
    .scl_io_num = CONFIG_APP_TOUCH_PIN_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
i2c_new_master_bus(&bus_config, &i2c_handle);

// Scan with probe
esp_err_t ret = i2c_master_probe(bus_handle, addr, 50);

// Clock speed moved to panel IO config
io_conf.scl_speed_hz = CONFIG_APP_TOUCH_I2C_CLOCK_HZ;

// Direct handle pass (no casting!)
esp_lcd_new_panel_io_i2c(i2c_handle, &io_conf, &tp_io);

// Store handle for cleanup
out->i2c_bus = i2c_handle;
```

## Benefits

1. ✅ **Future-proof** - Legacy I2C API is deprecated and will eventually be removed
2. ✅ **Better performance** - New driver has optimizations
3. ✅ **Type safety** - No more casting `i2c_port_t` to `esp_lcd_i2c_bus_handle_t`
4. ✅ **Cleaner code** - Simpler I2C scan, proper handle types
5. ✅ **Better pullup control** - Built into bus config flags
6. ✅ **Consistent with examples** - Matches ESP-LVGL-Port touchscreen examples
7. ✅ **Fixed header bug** - FT6x36 driver now includes correct header

## Testing Instructions

### For GT911 Touch (RGB 7" Display)
```bash
# Clean configuration
del sdkconfig  # or rm sdkconfig on Linux/Mac
idf.py fullclean

# Build and flash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_rgb_7inch" build flash monitor
```

### For FT6x36 Touch (SPI Display - if hardware available)
```bash
# Clean configuration
del sdkconfig  # or rm sdkconfig on Linux/Mac
idf.py fullclean

# Build and flash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_ili9341" build flash monitor
```

## Expected Behavior

### Build Time
- ✅ No deprecation warnings about legacy I2C API
- ✅ Clean compilation

### Runtime Logs
Look for these log messages:

```
I (xxx) app_touch: I2C master bus ready
I (xxx) app_touch: Scanning I2C bus...
I (xxx) app_touch: I2C device found at 0x5D    # GT911 address (or 0x38 for FT6x36)
```

For FT6x36, you should also see:
```
I (xxx) app_touch: Touch reset done
I (xxx) app_touch: Scanning I2C bus...
I (xxx) app_touch: I2C device found at 0x38
I (xxx) app_touch: FT6x36 touch init OK (addr=0x38)
```

### Touch Functionality
- ✅ Touch coordinates appear when touching screen
- ✅ Touch dot follows finger in hardware test UI
- ✅ No I2C communication errors
- ✅ Same behavior as before migration (no regressions)

## Critical Preservation

### FT6x36 Manual Reset Timing
The FT6x36 manual reset sequence was **preserved unchanged**:
- Reset pulse: 20ms LOW
- Boot delay: 250ms after reset HIGH
- This timing is critical for chip initialization
- Without it, chip ID registers read as 0x00

## Cleanup (Future Enhancement)

The migration stores `i2c_bus` handle in the structure but doesn't implement cleanup yet. For complete resource management, add a cleanup function:

```c
esp_err_t app_touch_deinit(app_touch_t *touch)
{
    if (touch->tp) esp_lcd_touch_del(touch->tp);
    if (touch->tp_io) esp_lcd_panel_io_del(touch->tp_io);
    if (touch->i2c_bus) i2c_del_master_bus(touch->i2c_bus);
    return ESP_OK;
}
```

This is not currently needed since the application runs continuously, but good practice for future refactoring.

## Verification Checklist

- [x] Both header files updated with I2C master handle
- [x] GT911 driver migrated to new API
- [x] FT6x36 driver migrated to new API
- [x] Wrong header include fixed (i2c_slave.h → i2c_master.h)
- [x] Manual reset timing preserved for FT6x36
- [x] I2C scan updated to use i2c_master_probe()
- [x] No type casting needed
- [x] Clock speed moved to panel IO config
- [x] Bus handles stored in structure
- [x] No legacy I2C API references remain

## Next Steps

1. Build the project for your target board
2. Flash and monitor to verify I2C initialization
3. Test touch functionality
4. Verify no regressions compared to previous version
5. Consider adding cleanup function for proper resource management (optional)

## References

- ESP-IDF I2C Master Driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- ESP-LVGL-Port touchscreen example: `managed_components/espressif__esp_lvgl_port/examples/touchscreen/main/main.c`
