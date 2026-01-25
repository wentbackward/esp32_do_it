# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP-IDF project template for ESP32 displays with LVGL It uses a hardware abstraction layer (HAL) pattern to support multiple display and touch drivers through configuration-driven build-time selection. The codebase is designed for production ESP32 UI products with clean separation between hardware, LVGL integration, and application UI.

## Why This Project Exists

This project takes a different approach from platforms like Arduino and PlatformIO, which attempt to accommodate all boards but become cumbersome and difficult to change. Key motivations:

**Avoiding Platform Bloat**: Arduino and PlatformIO try to be universal, creating abstraction layers that add friction when you need to customize or upgrade. This project starts specifically with ESP32 boards and embraces knowing your hardware.

**Rapid Upgrades**: Most ESP32 examples online are tied to old versions. LVGL is particularly problematic - most platforms are stuck on v8, while this project uses v9.4 or later. By using ESP-IDF directly, upgrading to new versions (like ESP-IDF 6 when it releases from beta) becomes straightforward rather than waiting for platform maintainers.

**Hardware Acceleration Access**: New ESP-IDF versions bring hardware accelerations and optimizations. Being tied to platform release cycles delays access to these improvements.

**Configuration Over Abstraction**: Board availability and peripheral combinations are increasingly complex. Instead of hiding hardware details (which you'll eventually need to understand anyway), this project uses a configurable setup via Kconfig. You embrace knowing your board rather than fighting abstraction layers.

**Board Evolution**: Boards change between iterations - different display drivers, touch controllers, pin mappings. A configuration-first approach handles this better than trying to maintain board-specific code for every variant.

**Compatibility Without Constraint**: While this project avoids platform bloat, it doesn't prevent using Arduino or PlatformIO if needed. The focus is on not being constrained by their limitations while maintaining a clean, upgradeable foundation.

In embedded development, you can't escape knowing your hardware. This project acknowledges that reality upfront, resulting in less friction when adopting the latest ESP-IDF, LVGL, or handling hardware configuration changes.

## Build System & Configuration

### Building for Different Boards

The project uses `sdkconfig.defaults.*` files for board-specific configurations. To build:

```bash
# In all cases
idf.py fullclean

# For ESP32-S3 + ILI9341 SPI display
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_ili9341" build

# For ESP32-S3 + RGB parallel display (800x480)
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_rgb" build

# Generic build (uses base defaults)
idf.py build flash monitor
```

### Switching Between Boards

**CRITICAL**: When switching boards, you MUST delete `sdkconfig`:

```bash
# Delete sdkconfig (required - fullclean doesn't do this!)
rm sdkconfig  # or del sdkconfig on Windows

# Clean build artifacts
idf.py fullclean

# Reconfigure with new board defaults
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_rgb" reconfigure build
```

**Why delete sdkconfig?**
- `sdkconfig` is the generated configuration file that takes precedence over `sdkconfig.defaults`
- `idf.py fullclean` intentionally preserves `sdkconfig` (considered user configuration)
- `idf.py reconfigure` will NOT apply changes from `sdkconfig.defaults` if `sdkconfig` exists
- Only when `sdkconfig` is missing will ESP-IDF generate a new one from `sdkconfig.defaults`

**Alternative**: If you want to keep `sdkconfig` in version control for a specific board:
```bash
# Generate sdkconfig for a board
rm sdkconfig
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_rgb" reconfigure
# sdkconfig now contains the full configuration for this board
# Can commit to git if desired
```

### Configuration via Menuconfig

All hardware parameters are exposed through menuconfig:
```bash
idf.py menuconfig
```
Navigate to "App: Display + Touch + LVGL Template" for all app-specific settings.

**Getting a clean, board-specific menuconfig:**

Instead of manually configuring everything from scratch, use the board's sdkconfig defaults file:

```bash
# Delete the generated sdkconfig file (IMPORTANT: fullclean does NOT do this!)
rm sdkconfig  # or del sdkconfig on Windows

# Launch menuconfig with board defaults pre-loaded
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_rgb" menuconfig
```

This loads all the board-specific settings (pins, resolution, display type, etc.) so you start from a known-good configuration rather than ESP-IDF defaults. You can then tune specific settings without having to configure the entire board from scratch.

### Hardware Display Test Mode

For board bringup without LVGL, enable hardware test mode (default):
```bash
CONFIG_APP_UI_HW_DISPLAY_TEST=y
```
This runs RGB panel validation tests (color fills, color bars) without LVGL overhead. See HARDWARE_TEST.md for details.

## Architecture

### Layered Design

```
Application UI Layer (ui_*.c, LVGL demos)
        ↓
LVGL Platform Layer (app_lvgl.c - LVGL init, buffer, rotation)
        ↓
Hardware Abstraction Layer (app_display_*.c, app_touch_*.c)
        ↓
ESP-IDF Drivers (esp_lcd, esp_lvgl_port, esp_lcd_touch)
        ↓
Hardware (MCU, display panel, touch controller)
```

**Key Principle:** Drivers are selected at **build-time** via Kconfig, not runtime. Only selected drivers are compiled.

### Source Selection via CMake

`main/CMakeLists.txt` conditionally includes sources based on CONFIG flags:
- Display: `app_display_ili9341.c` OR `app_display_rgb.c`
- Touch: `app_touch_ft6x36.c` OR `app_touch_gt911.c`

### Hardware Abstraction Layer (HAL)

Each hardware type has a common interface header:
- `app_display.h` - display init/control functions
- `app_touch.h` - touch init functions

Implementations:
- Display: `app_display_ili9341.c` (SPI), `app_display_rgb.c` (RGB parallel)
- Touch: `app_touch_ft6x36.c` (I2C FT6x36), `app_touch_gt911.c` (I2C GT911)

### LVGL Integration

`app_lvgl.c` handles:
- LVGL initialization (tick, task, locks)
- Display/touch registration via `esp_lvgl_port`
- Buffer strategy (DMA, double buffering, size via `CONFIG_APP_LVGL_BUF_LINES`)
- Rotation mapping (`swap_xy`, `mirror_x`, `mirror_y`)
- Bridging Kconfig settings to LVGL runtime

### Display Rendering Path

```
LVGL Widget → LVGL Render → esp_lvgl_port → HAL (app_display_*.c)
→ esp_lcd driver → SPI/RGB DMA → Display Panel
```

### Touch Input Path

```
Touch Panel → I2C → esp_lcd_touch driver → HAL (app_touch_*.c)
→ esp_lvgl_port → LVGL Input Device → UI Event Handlers
```

## Configuration Philosophy

**Configuration-first workflow**: Use Kconfig/menuconfig to control hardware parameters, not hardcoded values in C files.

Key settings:
- Display type, resolution, pins
- Touch type, I2C settings, pins
- LVGL buffer lines, DMA, double buffering
- Rotation/mirroring flags
- UI mode selection (hw test, simple, demos, custom)

## Component Dependencies

Managed via `main/idf_component.yml`:
- `lvgl/lvgl: ==9.4.0` (LVGL graphics library)
- `espressif/esp_lvgl_port` (ESP-IDF ↔ LVGL bridge)
- `espressif/esp_lcd_ili9341` (ILI9341 SPI display driver)
- `cfscn/esp_lcd_touch_ft6x36` (FT6x36/FT6336 touch driver)
- `espressif/esp_lcd_touch_gt911` (GT911 touch driver)

Dependencies are automatically fetched on build.

## Display & Touch Hardware Notes

### FT6x36/FT6336 Touch Reset Timing

The FT6x36 touch controller requires manual reset with specific timing:
- Set `CONFIG_APP_TOUCH_MANUAL_RESET=y`
- Set `CONFIG_APP_TOUCH_RESET_PULSE_MS=20` (reset pulse duration)
- Set `CONFIG_APP_TOUCH_RESET_BOOT_MS=250` (boot delay after reset)
- Set `rst_gpio_num=-1` in driver config to prevent driver from re-resetting

Without this, the controller may not boot properly and ID registers read as 0x00.

### RGB Panel Special GPIOs (Elecrow ESP32-S3 Boards)

Elecrow V3.0 boards require specific GPIO control:
- **GPIO 2**: TFT_BL (backlight PWM) - Controlled by LEDC PWM for brightness adjustment
- **GPIO 38**: Control signal (set LOW) - Required for display operation

**Backlight PWM Configuration** (in `sdkconfig.defaults.esp32s3_rgb`):
- `CONFIG_APP_LCD_PIN_BL=2` - GPIO 2 is the backlight pin
- `CONFIG_APP_LCD_BL_PWM_ENABLE=y` - Enable PWM control
- `CONFIG_APP_LCD_BL_PWM_FREQ_HZ=5000` - 5 kHz PWM frequency
- `CONFIG_APP_LCD_BL_PWM_RESOLUTION=10` - 10-bit resolution (0-1023)
- `CONFIG_APP_LCD_BL_DEFAULT_DUTY=80` - Start at 80% brightness

The hardware test UI provides a slider to adjust backlight from 0-100% if PWM is enabled.

### Display Orientation & Rotation

Rotation control happens at **3 layers**:
1. Panel MADCTL register (hardware-level axis swap/mirror in GRAM)
2. `esp_lcd_panel_mirror()` / `esp_lcd_panel_swap_xy()` helpers
3. LVGL port rotation flags (`swap_xy`, `mirror_x`, `mirror_y`)

**Current approach**: Use LVGL port rotation flags + MADCTL tweaks for inversion. Avoid "fighting" multiple layers.

### Memory & Buffering

For **SPI displays** (ILI9341):
- LVGL draw buffers in internal RAM (DMA-capable if `CONFIG_APP_LVGL_BUFF_DMA=y`)
- Buffer size: `HRES * CONFIG_APP_LVGL_BUF_LINES * 2` bytes (RGB565)
- Double buffering (`CONFIG_APP_LVGL_DOUBLE_BUFFER=y`) reduces tearing
- SPI DMA streams buffer to display

For **RGB parallel displays** (800x480):
- Requires PSRAM (`CONFIG_SPIRAM=y`, configured automatically in board defaults)
- Uses bounce buffer for RGB transfer
- Full framebuffer mode possible (higher memory, less tearing)

### Memory Management Philosophy

**Avoid malloc/lv_malloc in application code**: Application code should NOT use `malloc()` or `lv_malloc()` directly. Let ESP-IDF and LVGL manage their own memory according to menuconfig settings (`CONFIG_SPIRAM_*`, `CONFIG_LV_MEM_SIZE`, etc.). This allows:
- Memory optimization without modifying application code
- Proper respect for PSRAM vs internal RAM policies
- Menuconfig-driven tuning for different board capabilities

**Use static allocation for app UI elements**: For application-specific UI data (like the hardware test grid), use static arrays. Example from `ui_hwtest.c`:

```c
// Grid rendering - single static array, no malloc
static lv_point_precise_t s_grid_points[MAX_GRID_POINTS];
```

This approach:
- Avoids competing with ESP-IDF/LVGL heap management
- Eliminates heap fragmentation from many small allocations
- Provides predictable memory footprint (compile-time, not runtime)
- Keeps heap free for dynamic content that LVGL manages

**Proportional grid spacing**: The hardware test UI calculates grid spacing proportionally based on screen dimensions to ensure major gridlines evenly divide both width and height. The algorithm:
1. Tries common spacing values (20, 40, 50, 80, 100 pixels)
2. Scores each based on even division and reasonable line count (4-10 major lines per dimension)
3. Selects the spacing that divides both dimensions evenly with minimal remainder
4. Example: 800x480 screen uses 80px major grid (10×6 major lines), 240x320 uses 40px (6×8 major lines)
5. Minor grid is always major/4 for consistent visual density
6. Displays total line count in UI (each line is an LVGL object that affects FPS)

**Performance impact**: Each grid line is a separate LVGL line object. More lines = more objects to render = lower FPS. The algorithm targets 4-10 major lines per dimension to balance visual utility with rendering performance. The UI displays "Grid: N lines (AxBpx)" showing total line count - useful for understanding FPS behavior. On large displays (800x480), fewer, wider-spaced lines maintain good performance.

This ensures the grid looks clean on any display size without hardcoded values while maintaining good rendering performance.

**Why this matters**: On ESP32, menuconfig controls whether heap uses PSRAM, internal RAM, or both. Direct malloc/lv_malloc calls bypass these policies. Static allocation for app data + deferring to LVGL for UI objects = clean separation that respects menuconfig settings.

### LVGL Memory Allocator Selection

Different boards use different LVGL memory allocators based on their capabilities:

**For boards WITH PSRAM** (RGB 800x480):
- Use `CONFIG_LV_USE_CLIB_MALLOC=y` (C library malloc)
- LVGL calls standard `malloc()`/`realloc()`/`free()`
- ESP-IDF's malloc automatically uses PSRAM for large allocations (>16 KB)
- Small/critical allocations (<16 KB) stay in fast internal RAM
- **Benefits**: No fixed memory pool limit - LVGL can use all available PSRAM (~8 MB)
- **Configuration**: See `sdkconfig.defaults.esp32s3_rgb`

**For boards WITHOUT PSRAM** (SPI ILI9341 240x320):
- Use `CONFIG_LV_USE_BUILTIN_MALLOC=y` with `CONFIG_LV_MEM_SIZE_KILOBYTES=128`
- LVGL uses its own fixed memory pool (128 KB)
- Sufficient for simple UIs on smaller displays
- **Configuration**: See `sdkconfig.defaults.esp32s3_ili9341`

**How ESP-IDF's malloc works with PSRAM**:
When `CONFIG_SPIRAM_USE_MALLOC=y` is enabled:
- `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384` - First 16 KB of any allocation uses internal RAM
- Large allocations automatically go to PSRAM
- Critical operations (ISR, DMA) stay in internal RAM
- No application code changes needed - transparent to LVGL

**Why this strategy**:
- PSRAM boards: Maximize available memory for complex UIs (MBs instead of fixed 128 KB)
- Non-PSRAM boards: Predictable memory usage with fixed pool
- Configuration-driven: No code changes, just sdkconfig settings
- Respects ESP-IDF memory policies: Smart allocation based on size and usage

## UI Modes

Selected via Kconfig `CONFIG_APP_UI_*`:

1. **Hardware Display Test** (`CONFIG_APP_UI_HW_DISPLAY_TEST=y`): RGB panel validation without LVGL (color fills, color bars). For board bringup.

2. **Hardware Test UI** (`CONFIG_APP_UI_HWTEST=y`): LVGL-based hardware diagnostics with:
   - Proportional grid (auto-calculated for even screen division)
   - Grid info label showing line count and spacing (e.g., "Grid: 68 lines (20x80px)")
   - Corner markers (TL/TR/BL/BR) for orientation verification
   - Color swatches (R/G/B/W/C/M/Y/K) for color accuracy
   - Test button (center) - click counter to verify touch/click events work correctly
   - Touch dot and coordinate display - shows exact touch coordinates
   - Moving bar for tearing/flicker detection
   - FPS counter
   - Optional controls (displayed only if hardware supports):
     - Invert button (SPI displays)
     - Orientation cycle button (SPI displays)
     - Backlight slider (if PWM enabled)

3. **Simple UI** (`CONFIG_APP_UI_SIMPLE=y`): Minimal LVGL demo (color bars, label, button). See `ui_simple_start()` in `main/main.c`.

4. **LVGL Demos** (`CONFIG_APP_UI_DEMO=y`): Built-in LVGL demos (widgets, benchmark, music). Requires enabling `LV_USE_DEMO_*` in menuconfig.

## Adding Support for New Hardware

### New Display Driver

1. Create `app_display_mydriver.c` and `app_display_mydriver.h`
2. Implement `app_display_init()` and control functions
3. Add Kconfig option in `main/Kconfig.projbuild`
4. Update `main/CMakeLists.txt` to conditionally include the new source
5. Create `sdkconfig.defaults.myboard` with board-specific settings

### New Touch Driver

1. Create `app_touch_mydriver.c` and `app_touch_mydriver.h`
2. Implement `app_touch_init()`
3. Add Kconfig option in `main/Kconfig.projbuild`
4. Update `main/CMakeLists.txt` to conditionally include the new source
5. Update board's sdkconfig file

### New Board Configuration

1. Copy an existing `sdkconfig.defaults.boardname` file
2. Update display/touch settings, pins, resolution
3. Add any board-specific GPIO initialization to display HAL
4. Test with hardware display test mode first
5. Document in BOARDS.md and MODULES.md

### Hardware Capability Abstraction

Display drivers expose optional hardware features via function pointers:
- `app_display_set_invert()` - Invert display colors (SPI panels only)
- `app_display_cycle_orientation()` - Change orientation (SPI panels only)
- `app_display_set_backlight_percent()` - PWM backlight control (if configured)

**Pattern for new display types:**

1. In your display HAL (`app_display_mydriver.c`), implement only the functions your hardware supports. For unsupported features, either omit the function or return `false`.

2. In `main.c`, conditionally set function pointers based on display type:
```c
hwtest_cfg_t hwcfg = {
    .hres = CONFIG_APP_LCD_HRES,
    .vres = CONFIG_APP_LCD_VRES,
#if CONFIG_APP_DISPLAY_SUPPORTS_INVERT
    .set_invert = app_display_set_invert,
#else
    .set_invert = NULL,
#endif
    // ... other settings
};
```

3. The UI (`ui_hwtest.c`) automatically creates controls only for non-NULL function pointers. No buttons appear for unsupported features.

**Why this matters:** RGB parallel displays don't support invert or orientation commands (these require panel IO commands). SPI displays do. By passing NULL for unsupported features, the UI stays clean and hardware-appropriate.

## Common Development Tasks

### Configuring a Board

To configure a board using its pre-defined settings instead of starting from scratch:

```bash
# IMPORTANT: Delete sdkconfig first (fullclean does NOT do this!)
rm sdkconfig  # or del sdkconfig on Windows

# Load board defaults and open menuconfig for fine-tuning
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_ili9341" menuconfig

# Build with the configuration
idf.py build flash monitor
```

This approach loads all board-specific settings (display type, pins, resolution, touch configuration) so you start from a known-good baseline. You can then adjust specific parameters without manually configuring everything.

**Important**: `sdkconfig` takes precedence over `sdkconfig.defaults`. If `sdkconfig` exists, your changes to `sdkconfig.defaults.*` files will be ignored. Always delete `sdkconfig` when switching boards or updating board defaults.

**Tip:** The generated `sdkconfig` file can be committed if you want to preserve specific tweaks, or kept in `.gitignore` if you prefer to always generate from defaults.

### Updating LVGL Configuration

LVGL configuration is managed through `lv_conf.h` (or via Kconfig in some setups). For buffer size, rotation, color depth:
```bash
idf.py menuconfig
# Navigate to "App: Display + Touch + LVGL Template"
```

### Testing Display Without UI

Use hardware test mode to isolate display issues:
1. Set `CONFIG_APP_UI_HW_DISPLAY_TEST=y`
2. Build and flash
3. Verify color bars appear correctly
4. Switch to LVGL UI mode after validation

### Debugging Touch Issues

1. Check I2C bus with logic analyzer or scope
2. Verify reset timing (manual reset enabled, proper delays)
3. Check that vendor/chip ID registers are read correctly (not 0x00)
4. Enable ESP-IDF I2C debug logs: `idf.py menuconfig → Component config → ESP32-specific → I2C`
5. Verify touch coordinates map correctly to display (check rotation settings)

### Performance Tuning

- Increase `CONFIG_APP_LVGL_BUF_LINES` for fewer flush calls (uses more RAM)
- Enable `CONFIG_APP_LVGL_DOUBLE_BUFFER=y` for smoother rendering
- Increase SPI clock (`CONFIG_APP_LCD_SPI_CLOCK_HZ`) if supported by panel
- For RGB displays, adjust pixel clock and timings
- Use PSRAM for large displays/framebuffers

## Documentation Structure

- `README.md` - Quick start guide for template users
- `BOARDS.md` - Board-specific build instructions and configuration guide
- `ARCHITECTURE.md` - High-level layered architecture diagram
- `DETAILED_ARCHITECTURE.md` - Deep technical architecture (DMA, memory, pipelines)
- `MODULES.md` - Tested hardware modules and their configurations
- `HARDWARE_TEST.md` - Hardware display test mode usage and troubleshooting
- `docs/TECH_NOTES.md` - Additional technical notes (if exists)

## ESP-IDF Version

This project targets **ESP-IDF v5.x** (tested with v5.5.2). Ensure ESP-IDF environment is activated:
```bash
idf.py --version
```

## Common Pitfalls

1. **Not deleting `sdkconfig` when switching boards or updating defaults**: `idf.py fullclean` does NOT delete `sdkconfig`. When `sdkconfig` exists, it takes precedence over `sdkconfig.defaults`, so changes to board defaults files won't be applied. **Solution**: Always `rm sdkconfig` (or `del sdkconfig` on Windows) before switching boards or when you've updated `sdkconfig.defaults.*` files. This is the most common source of confusion when board configurations don't apply.

2. **Not using manual reset for FT6x36**: Results in 0x00 chip IDs and non-functional touch
3. **Wrong rotation layer**: Applying rotation at multiple layers can cause strange coordinate mapping
4. **Insufficient buffer lines**: Too-small `CONFIG_APP_LVGL_BUF_LINES` causes excessive flushes and poor performance
5. **Missing PSRAM for large displays**: RGB 800x480 displays need PSRAM enabled
6. **Hardcoding values instead of using Kconfig**: Breaks configuration-driven workflow
7. **Not setting NULL for unsupported features**: When adding new display types, set function pointers to NULL in `main.c` for features the hardware doesn't support (e.g., RGB displays can't do invert/orientation via panel commands). This prevents kernel panics and keeps the UI clean.
8. **Using malloc/lv_malloc in application code**: Don't use `malloc()` or `lv_malloc()` directly in application code. This bypasses ESP-IDF and LVGL memory management configured via menuconfig (PSRAM policies, heap sizes, etc.). Use static allocation for application data structures instead. Let ESP-IDF and LVGL handle their own dynamic allocations according to menuconfig settings. Example: The hardware test UI uses `static lv_point_precise_t s_grid_points[400]` instead of 150+ individual `lv_malloc()` calls.
