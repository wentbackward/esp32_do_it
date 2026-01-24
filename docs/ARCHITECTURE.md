# System Architecture Diagram

## 1) Layered Architecture Overview

```
┌───────────────────────────────────────────────────────────────┐
│                       Application Layer                      │
│                                                             │
│  UI Modules                                                  │
│  ├─ ui_hwtest.c   (hardware diagnostics & validation)         │
│  ├─ ui_simple.c   (minimal demo UI)                           │
│  └─ LVGL demos    (widgets, benchmark, music)                 │
│                                                             │
└───────────────────────────────────────────────────────────────┘
                            ▲
                            │ LVGL API
                            ▼
┌───────────────────────────────────────────────────────────────┐
│                     LVGL Platform Layer                      │
│                                                             │
│  app_lvgl.c                                                │
│  ├─ LVGL init (tick, task, locks)                            │
│  ├─ Display registration (esp_lvgl_port)                     │
│  ├─ Touch registration (esp_lvgl_port)                       │
│  ├─ Buffer strategy (DMA, double buffer, size)               │
│  ├─ Rotation mapping (swap_xy, mirror_x, mirror_y)           │
│  └─ LVGL configuration bridge from Kconfig                   │
│                                                             │
└───────────────────────────────────────────────────────────────┘
                            ▲
                            │ HAL API
                            ▼
┌───────────────────────────────────────────────────────────────┐
│                Hardware Abstraction Layer (HAL)              │
│                                                             │
│  Display HAL                                                 │
│  ├─ app_display_ili9341.c  (SPI ILI9341)                     │
│  ├─ app_display_rgb.c      (future ESP32-P4 RGB)             │
│  ├─ app_display_mipi.c     (future ESP32-P4 MIPI DSI)        │
│  └─ app_display.h          (common interface)                │
│                                                             │
│  Touch HAL                                                   │
│  ├─ app_touch_ft6x36.c     (I2C FT6x36/FT6336)               │
│  ├─ app_touch_gt911.c      (future GT911)                    │
│  ├─ app_touch_none.c       (null driver)                     │
│  └─ app_touch.h            (common interface)                │
│                                                             │
└───────────────────────────────────────────────────────────────┘
                            ▲
                            │ esp_lcd / drivers
                            ▼
┌───────────────────────────────────────────────────────────────┐
│                 ESP-IDF Driver & Middleware Layer            │
│                                                             │
│  Display Drivers                                             │
│  ├─ esp_lcd_ili9341                                          │
│  ├─ esp_lcd_panel_io_spi                                    │
│  └─ (future) RGB / MIPI panel drivers                        │
│                                                             │
│  Touch Drivers                                               │
│  ├─ esp_lcd_touch_ft6x36                                    │
│  └─ (future) GT911 driver                                   │
│                                                             │
│  LVGL Port                                                   │
│  └─ esp_lvgl_port                                           │
│                                                             │
│  Core Drivers                                                │
│  ├─ SPI (spi_master)                                        │
│  ├─ I2C (i2c driver)                                        │
│  ├─ GPIO                                                    │
│  └─ DMA                                                     │
│                                                             │
└───────────────────────────────────────────────────────────────┘
                            ▲
                            │ hardware buses
                            ▼
┌───────────────────────────────────────────────────────────────┐
│                       Hardware Layer                         │
│                                                             │
│  MCU                                                         │
│  ├─ ESP32-S3 (current target)                               │
│  └─ ESP32-P4 (future target)                                │
│                                                             │
│  Display                                                      │
│  ├─ ILI9341 (SPI, 240x320)                                  │
│  ├─ RGB TFT (future)                                        │
│  └─ MIPI DSI panel (future)                                 │
│                                                             │
│  Touch                                                       │
│  ├─ FT6x36 / FT6336 (I2C)                                   │
│  └─ GT911 (future)                                          │
│                                                             │
└───────────────────────────────────────────────────────────────┘
```

---

## 2) Configuration & Build-Time Control Flow

```
Kconfig.projbuild
   │
   ▼
sdkconfig / CONFIG_APP_*
   │
   ▼
CMakeLists.txt (conditional source inclusion)
   │
   ▼
HAL driver selection
   │
   ▼
app_main()
   ├─ app_display_init()
   ├─ app_touch_init() (optional)
   ├─ app_lvgl_init_and_add()
   └─ UI selection (hwtest / simple / demo)
```

Key idea:

> **Drivers are selected at build-time, not runtime.**

---

## 3) Data & Event Flow (Runtime)

### Display rendering path

```
LVGL Widget → LVGL Render Engine → esp_lvgl_port →
app_display HAL → esp_lcd driver → SPI DMA → ILI9341 Panel
```

### Touch input path

```
FT6x36 Touch Panel → I2C →
esp_lcd_touch_ft6x36 →
app_touch HAL →
esp_lvgl_port →
LVGL Input Device →
UI Event Handlers
```

---

## 4) Board Profile & Future ESP32-P4 Expansion

```
Board Profile (Kconfig)
   ├─ ESP32-S3 + ILI9341 + FT6x36   ✅ implemented
   └─ ESP32-P4 + RGB/MIPI + GT911   ⏳ planned
```

Future changes affect only:

* Display HAL implementation
* Touch HAL implementation
* LVGL buffer strategy (PSRAM / framebuffer)

UI and application logic remain unchanged.

---

## 5) Architectural Principles Visualized

```
UI Layer           ← never touches hardware directly
   ↓
LVGL Platform      ← knows LVGL + HAL, not hardware details
   ↓
HAL Layer          ← knows hardware type, not UI logic
   ↓
ESP-IDF Drivers    ← vendor drivers
   ↓
Hardware
```

This enforces:

* Loose coupling
* Driver swapability
* ESP32-P4 readiness
* Maintainability
