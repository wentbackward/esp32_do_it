# LovyanGFX Integration Guide

This document explains how to use LovyanGFX as a display driver in this ESP-IDF project.

## Overview

LovyanGFX has been integrated as a third display driver option alongside the existing esp_lcd-based drivers (ILI9341 SPI and RGB Parallel). It provides:

- **Native ESP-IDF support** - No Arduino framework required
- **Optimized performance** - Hardware-accelerated DMA operations
- **Unified API** - Same interface for SPI and RGB panels
- **Manufacturer configs** - Easy to use vendor-provided configurations
- **LVGL integration** - Seamless integration with LVGL 9.4

## Architecture

LovyanGFX is integrated following the project's HAL pattern:

```
Display Drivers (select one at build time):
├── app_display_ili9341.c  (esp_lcd - SPI)
├── app_display_rgb.c      (esp_lcd - RGB parallel)
└── app_display_lgfx.cpp   (LovyanGFX - SPI or RGB)
```

The HAL provides the same `app_display_t` interface regardless of driver:
- `app_display_init()` - Initialize display
- `app_display_set_invert()` - Toggle color inversion
- `app_display_cycle_orientation()` - Change screen rotation
- `app_display_set_backlight_percent()` - Adjust backlight

## Building with LovyanGFX

### Option 1: Using Pre-configured Board Defaults

For the Elecrow 7" 800x480 RGB display:

```bash
# IMPORTANT: Delete sdkconfig when switching drivers!
rm sdkconfig  # or: del sdkconfig on Windows

# Clean build
idf.py fullclean

# Build with LovyanGFX
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_lgfx_7inch" build

# Flash and monitor
idf.py flash monitor
```

### Option 2: Manual Configuration via Menuconfig

```bash
# Delete sdkconfig
rm sdkconfig

# Start with base defaults
idf.py menuconfig

# Navigate to: App: Display + Touch + LVGL Template
# - Display driver -> LovyanGFX (supports SPI and RGB panels)
# - Display: LovyanGFX
#   - LovyanGFX panel type -> RGB parallel panel (or SPI panel)
#   - Use custom lgfx_user.hpp -> No (uses auto-config from Kconfig)

# Configure display resolution, pins, and timing in the standard menus
# - Display common (resolution, color depth, etc.)
# - Display: RGB Parallel (pin mappings, timings)

# Save and build
idf.py build flash monitor
```

## Configuration Modes

LovyanGFX supports two configuration modes:

### 1. Auto-Configuration (Default)

When `CONFIG_APP_LGFX_USE_CUSTOM_CONFIG=n` (default):
- Configuration is generated from Kconfig settings
- Uses `main/lgfx_auto_config.hpp`
- All settings come from menuconfig (pins, timings, resolution)
- Best for standard boards with straightforward configurations

### 2. Custom Configuration

When `CONFIG_APP_LGFX_USE_CUSTOM_CONFIG=y`:
- You provide `main/lgfx_user.hpp` with custom LGFX configuration
- Full control over LovyanGFX setup
- Useful for complex boards or manufacturer-provided configs

**Example: Using the Elecrow 7" custom config**

```bash
# Copy the example configuration
cp main/lgfx_elecrow_7inch.hpp main/lgfx_user.hpp

# Enable custom config in menuconfig
idf.py menuconfig
# Set: Display: LovyanGFX -> Use custom lgfx_user.hpp -> Yes

# Build
idf.py build
```

## Creating Custom Configurations

To create a configuration for a new board:

1. **Copy the example:**
   ```bash
   cp main/lgfx_elecrow_7inch.hpp main/lgfx_mynewboard.hpp
   ```

2. **Edit the configuration:**
   - Update pin mappings (`cfg.pin_d0` through `cfg.pin_d15`)
   - Adjust timing parameters (hsync, vsync, pclk)
   - Set resolution (`cfg.memory_width`, `cfg.memory_height`)
   - Configure backlight if using PWM

3. **Use it:**
   ```bash
   cp main/lgfx_mynewboard.hpp main/lgfx_user.hpp
   # Enable CONFIG_APP_LGFX_USE_CUSTOM_CONFIG=y in menuconfig
   ```

## Switching Between Drivers

When switching between display drivers (esp_lcd ↔ LovyanGFX):

1. **Delete sdkconfig** (CRITICAL!)
   ```bash
   rm sdkconfig  # or: del sdkconfig on Windows
   ```

2. **Clean build artifacts**
   ```bash
   idf.py fullclean
   ```

3. **Reconfigure and build**
   ```bash
   idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_lgfx_7inch" build
   ```

**Why delete sdkconfig?**
- `idf.py fullclean` intentionally preserves `sdkconfig`
- `sdkconfig` takes precedence over `sdkconfig.defaults`
- Display driver selection is a build-time choice - must fully reconfigure

## Performance Tips

### Memory Allocation

For RGB displays (800x480):
- PSRAM is required (`CONFIG_SPIRAM=y`)
- Use C library malloc for LVGL (`CONFIG_LV_USE_CLIB_MALLOC=y`)
- ESP-IDF automatically places large allocations in PSRAM
- Buffer size: `CONFIG_APP_LVGL_BUF_LINES=40` is a good starting point

### Optimization Flags

Enable these for better performance:
```
CONFIG_GPIO_CTRL_FUNC_IN_IRAM=y
CONFIG_LCD_RGB_ISR_IRAM_SAFE=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
```

### LovyanGFX vs esp_lcd Performance

LovyanGFX often provides:
- Faster pixel pushing (optimized DMA)
- Better color conversion
- More efficient memory usage
- Smoother animations with LVGL

Benchmark your specific use case to verify performance gains.

## Troubleshooting

### Build Errors

**Error: `LGFX class not found`**
- Ensure component dependency is fetched: `idf.py reconfigure`
- Check that `lovyan03/lovyangfx` appears in `managed_components/`

**Error: `undefined reference to app_display_get_lgfx`**
- CMakeLists.txt must include `app_display_lgfx.cpp` (not `.c`)
- Verify `CONFIG_APP_DISPLAY_LGFX=y` is set

### Display Issues

**Blank screen after flashing:**
- Check backlight GPIO and PWM configuration
- Verify RGB pin mappings match your board
- Confirm PSRAM is enabled for 800x480 displays
- Check timing parameters (hsync/vsync)

**Colors are wrong:**
- Try toggling `CONFIG_APP_LCD_BGR`
- Check `CONFIG_APP_LCD_SWAP_BYTES`
- Verify RGB data pin order (D0-D15)

**Display is rotated/mirrored:**
- Use `app_display_cycle_orientation()` to test rotations
- Adjust rotation settings in code or via LGFX's `setRotation()`

## Comparison with esp_lcd Drivers

| Feature | esp_lcd (ILI9341 SPI) | esp_lcd (RGB) | LovyanGFX |
|---------|----------------------|---------------|-----------|
| SPI panels | ✅ | ❌ | ✅ |
| RGB panels | ❌ | ✅ | ✅ |
| Rotation/invert | ✅ | ❌ | ✅ |
| Performance | Good | Good | Excellent |
| Memory efficiency | Good | Good | Better |
| Configuration | Kconfig | Kconfig | Kconfig or C++ |
| LVGL integration | Via esp_lvgl_port | Custom | Custom |
| Arduino dependency | No | No | No |

## When to Use LovyanGFX

**Choose LovyanGFX when:**
- You have a manufacturer-provided LGFX config
- You need maximum performance
- You want unified API for SPI and RGB panels
- You need advanced features (rotation, inversion) on RGB panels

**Stick with esp_lcd when:**
- You prefer the official ESP-IDF driver stack
- Your board works well with existing esp_lcd drivers
- You want to minimize custom code

## Future Enhancements

Potential improvements to the LovyanGFX integration:

1. **SPI panel configurations** - Add example configs for ILI9341, ST7789, etc.
2. **Touch integration** - Add LovyanGFX touch driver support
3. **Hardware acceleration** - Leverage LGFX's drawing primitives
4. **More board configs** - Add pre-configured settings for popular boards

## References

- [LovyanGFX GitHub](https://github.com/lovyan03/LovyanGFX)
- [LovyanGFX Documentation](https://github.com/lovyan03/LovyanGFX/tree/master/doc)
- [LVGL Documentation](https://docs.lvgl.io/)
- Project CLAUDE.md for architecture details
