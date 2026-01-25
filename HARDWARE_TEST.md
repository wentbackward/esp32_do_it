# Hardware Display Test Mode

This project includes a hardware-only display test mode that validates RGB panel functionality without LVGL. This is useful for:
- Bringing up new display boards
- Debugging display issues
- Verifying GPIO configurations
- Testing display before adding UI complexity

## What It Tests

The hardware test runs a sequence of visual tests:

1. **Red Fill** - Entire screen filled with red (2 seconds)
2. **Green Fill** - Entire screen filled with green (2 seconds)
3. **Blue Fill** - Entire screen filled with blue (2 seconds)
4. **Color Bars** - 8 horizontal bars showing: Red, Green, Blue, Yellow, Magenta, Cyan, White, Black (stays on screen)

## How to Enable

### Option 1: Menuconfig
```bash
idf.py menuconfig
```
Navigate to: `App: Display + Touch + LVGL Template → UI mode`
Select: `Hardware Display Test (no LVGL - RGB panel validation)`

### Option 2: sdkconfig
In your board's `sdkconfig.defaults.xxx` file, set:
```
CONFIG_APP_UI_HW_DISPLAY_TEST=y
```

### Option 3: Command line
```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_rgb" build
```
(The default config already enables hardware test mode)

## Switching to LVGL UI

Once the hardware test passes and you see color bars on screen:

1. Run menuconfig: `idf.py menuconfig`
2. Navigate to: `App: Display + Touch + LVGL Template → UI mode`
3. Select one of:
   - `Hardware test UI with LVGL` - LVGL-based hardware test
   - `Simple UI` - Basic LVGL demo
   - `LVGL demo widgets` - Full LVGL widget showcase
4. Build and flash: `idf.py build flash monitor`

## Expected Output

If working correctly, you should see:
```
I (865) app_display: GPIO 2 (display power) set HIGH
I (865) app_display: GPIO 38 (control) set LOW
I (865) app_display: RGB panel config: 800x480, PCLK=15000000 Hz, bounce_buf=32000 px
I (872) app_display: RGB panel created
I (876) app_display: Panel reset complete
I (880) app_display: Panel init complete
I (884) hw_display_test: === RGB Panel Hardware Test ===
I (890) hw_display_test: Panel: 0x3fcxxxxx, Resolution: 800x480
I (896) hw_display_test: Test 1/4: Fill screen RED
I (3902) hw_display_test: RED fill complete
I (3902) hw_display_test: Test 2/4: Fill screen GREEN
I (5906) hw_display_test: GREEN fill complete
I (5906) hw_display_test: Test 3/4: Fill screen BLUE
I (7910) hw_display_test: BLUE fill complete
I (7910) hw_display_test: Test 4/4: 8 color bars
I (8918) hw_display_test: === Hardware test complete ===
I (8918) hw_display_test: If you see this, RGB panel hardware is working correctly!
```

## Troubleshooting

### Display stays black
- Check power connections
- Verify GPIO 2 and GPIO 38 are configured correctly (board-specific)
- Check backlight enable pin
- Verify RGB data pins match your board

### Colors are wrong
- Check BGR vs RGB setting in `CONFIG_APP_LCD_BGR`
- Verify byte swapping setting

### Garbled/shifted display
- Check timing parameters (hsync/vsync porches)
- Verify pixel clock frequency
- Check data pin mapping (D0-D15)

### Works but LVGL doesn't
- Ensure LVGL memory is configured to use PSRAM
- Check `CONFIG_LV_USE_BUILTIN_MALLOC=y` in sdkconfig
- Verify SPIRAM is enabled and detected

## Board-Specific Notes

### Elecrow ESP32-S3 5" 800x480
- **GPIO 2**: Display power/backlight (must be HIGH)
- **GPIO 38**: Control signal (must be LOW)
- RGB timings already configured in `sdkconfig.defaults.esp32s3_rgb`

### Adding Your Board
1. Create `sdkconfig.defaults.yourboard`
2. Configure display resolution, GPIO pins, and timings
3. Add any board-specific GPIO initialization to `app_display_rgb.c`
4. Test with hardware test mode first
5. Then enable LVGL UI mode
