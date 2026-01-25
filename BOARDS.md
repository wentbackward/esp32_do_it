# Board Configuration Guide

This project supports multiple hardware boards using separate sdkconfig files.

## Available Boards

### ESP32-S3 + SPI ILI9341 + FT6x36
- Display: 240x320 ILI9341 over SPI
- Touch: FT6x36/FT6336 over I2C
- Config file: `sdkconfig.defaults.esp32s3_ili9341`

### ESP32-S3 + RGB 800x480 + GT911
- Display: 800x480 RGB parallel panel
- Touch: GT911 over I2C
- Requires PSRAM (configured automatically)
- Config file: `sdkconfig.defaults.esp32s3_rgb`

## How to Build for a Specific Board

### Option 1: Set board at build time
```bash
# For ILI9341 board
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_ili9341" build

# For RGB board
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_rgb" build
```

### Option 2: Set board permanently
Copy the board config to sdkconfig.defaults:
```bash
# For ILI9341 board
cp sdkconfig.defaults.esp32s3_ili9341 sdkconfig.board
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.board" build

# Or simply
cat sdkconfig.defaults.esp32s3_ili9341 >> sdkconfig.defaults
idf.py build
```

### Option 3: Use menuconfig
```bash
idf.py menuconfig
```
Navigate to "App: Display + Touch + LVGL Template" and manually configure your hardware.

## Switching Boards

When switching between boards, clean the build first:
```bash
idf.py fullclean
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3_rgb" reconfigure build
```

## Adding a New Board

1. Copy an existing board config file
2. Rename it (e.g., `sdkconfig.defaults.myboard`)
3. Edit the settings for your hardware
4. Use it with: `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.myboard" build`

## What Changed?

The old Kconfig used conditional defaults with board profiles, which caused:
- Menuconfig errors when profiles conflicted
- Duplicate config options
- Complex dependencies

The new approach:
- Simple Kconfig with no board-specific conditionals
- Board settings in separate sdkconfig files (industry standard)
- Easier to maintain and extend
- No more menuconfig conflicts
