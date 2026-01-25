# ESP32 + Display + Touch modules tested with this library
## Hosyond ESP32S3 2.8", 240x320
- SKU: ES3C28P
- SoC: ESP32 S3
- Display: ILI9431, 240 x 320 px, 16 Color, 80 MHz to 240 MHz
- Display bus: TFT_eSPI: 2.5.0
- Touch: FT6636 capacitive touch
- Memory: TBD
- Flash: 
- Tested with: ESP-IDF v 5.5.2, LVGL 9.4
- Key configuration notes: 
## ELECROW CrowPanel-ESP32 5", 800 x 480, Basic edition
- SKU: DIS07050H-1
- Board Version: V3.0
- SoC: ESP32-S3-WROOM-1-N4R8
- Display: 800x480 RGB parallel panel (ILI6122 controller)
- Touch: GT911 capacitive touch (I2C)
- Memory: 8 MB PSRAM (octal mode)
- Flash: 4 MB
- Backlight: GPIO 2 (TFT_BL) - PWM controlled
- Special GPIOs: GPIO 38 (control signal, must be LOW)
- Tested with: ESP-IDF v5.5.2, LVGL 9.4
- Wiki / Data sheets: https://www.elecrow.com/wiki/esp32-display-502727-intelligent-touch-screen-wi-fi26ble-800480-hmi-display.html
- Key configuration: Uses C library malloc for LVGL (dynamic PSRAM allocation)
## ELECROW CrowPanel-ESP32 7", 800 x 480
- SKU: DIS08070H
- Board Version: V3.0
- SoC: ESP32-S3-WROOM-1-N4R8
- Display: 800x480 RGB parallel panel (controller TBD)
- Display bus: RGB parallel interface, 15 MHz PCLK
- Touch: GT911 capacitive touch (I2C on pins 19/20)
- Memory: 8 MB PSRAM (octal mode)
- Flash: 4 MB
- Backlight: GPIO 2 (TFT_BL) - PWM controlled (assumed, not verified)
- Control pins: HSYNC=39, VSYNC=40, DE=41, PCLK=0
- RGB data pins: D0-D15 = 15,7,6,5,4,9,46,3,8,16,1,14,21,47,48,45
- Timing: HSYNC(40/48/40), VSYNC(1/31/13)
- Tested with: ESP-IDF v5.5.2, LVGL 9.4
- Key configuration: Uses C library malloc for LVGL (dynamic PSRAM allocation)
