Below is a **deep technical architecture diagram** (hardware + DMA + LVGL pipeline + memory + future ESP32-P4 paths). It’s structured so you can paste it into Markdown docs and it stays readable.

---

# Deep Technical Architecture Diagram

## 1) End-to-End Runtime Pipeline (Render + Input)

```
                            ┌────────────────────────────────────────────┐
                            │                 UI Layer                   │
                            │                                            │
                            │  ui_hwtest / ui_simple / lvgl demos        │
                            │  - creates objects                         │
                            │  - registers callbacks                      │
                            └───────────────┬────────────────────────────┘
                                            │ LVGL API calls
                                            ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                                   LVGL Core                                      │
│                                                                                  │
│  Object Tree  ──► Style Resolution ──► Layout ──► Draw Tasks ──► Draw Buffers     │
│                                                                                  │
│  lv_timer handler (periodic)                                                     │
│  - drives animation, invalidation, rendering                                     │
│                                                                                  │
│  Output: "flush requests" (areas)                                                │
└───────────────────────────────────┬──────────────────────────────────────────────┘
                                    │ lv_disp_flush (via display driver)
                                    ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                               esp_lvgl_port                                      │
│                                                                                  │
│  - Creates LVGL task / tick integration                                           │
│  - Provides lock/unlock (thread safety)                                           │
│  - Bridges LVGL display flush into esp_lcd panel driver                            │
│  - Bridges esp_lcd_touch -> LVGL indev read                                       │
│                                                                                  │
│  Display side                                                                     │
│  - allocates draw buffers (DMA capable if enabled)                                │
│  - handles double buffering (optional)                                            │
│  - performs optional byte swap (swap_bytes)                                       │
│  - pushes pixel data to esp_lcd_panel_draw_bitmap()                               │
│                                                                                  │
│  Input side                                                                       │
│  - polls touch via esp_lcd_touch_read_data()                                      │
│  - reports LV_INDEV_STATE_PRESSED/RELEASED + coordinates                          │
└───────────────────────────────────┬──────────────────────────────────────────────┘
                                    │ esp_lcd API calls
                                    ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                                   esp_lcd                                        │
│                                                                                  │
│  Panel IO layer (transport)                                                      │
│   - SPI: esp_lcd_panel_io_spi                                                     │
│     - manages transactions, DC toggling, CS, clock, queueing                      │
│     - uses SPI master driver                                                      │
│                                                                                  │
│  Panel driver (protocol + init)                                                   │
│   - ILI9341 panel driver (esp_lcd_ili9341)                                        │
│     - init sequence                                                               │
│     - MADCTL, inversion, pixel format, etc.                                       │
│                                                                                  │
│  Touch driver                                                                     │
│   - FT6x36 (esp_lcd_touch_ft6x36)                                                 │
│     - I2C register reads                                                          │
│     - coordinate decoding, point count                                            │
└───────────────────────────────────┬──────────────────────────────────────────────┘
                                    │ driver calls
                                    ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                           ESP-IDF peripheral drivers                             │
│                                                                                  │
│  SPI Master Driver                                                               │
│   - queue transactions                                                           │
│   - DMA descriptors                                                              │
│   - interrupts/callbacks                                                         │
│                                                                                  │
│  I2C Driver (legacy in current impl)                                             │
│   - i2c_master_cmd_begin()                                                       │
│   - start/write/stop sequences                                                   │
│                                                                                  │
│  GPIO Driver                                                                     │
│   - BL on/off or PWM                                                             │
│   - reset pins                                                                   │
│   - optional INT pin                                                             │
└───────────────────────────────────┬──────────────────────────────────────────────┘
                                    │ electrical buses
                                    ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                                     Hardware                                     │
│                                                                                  │
│  SPI bus → ILI9341 display                                                       │
│   - DC/RS, CS, SCK, MOSI, (optional MISO), RST, BL                               │
│                                                                                  │
│  I2C bus → FT6x36 / FT6336 touch                                                 │
│   - SDA, SCL, RST, (optional INT)                                                │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2) Memory & Buffering Model (LVGL + DMA + SPI)

### Current (ESP32-S3 + SPI ILI9341)

```
                   ┌──────────────────────────────────────┐
                   │               Internal RAM            │
                   │  (DMA-capable region when requested)  │
                   │                                      │
                   │  LVGL draw buffers                    │
                   │   - buf0: HRES * LINES * 2 bytes      │
                   │   - buf1: (optional)                  │
                   │                                      │
                   └───────────────┬──────────────────────┘
                                   │ pointer passed to
                                   │ esp_lcd_panel_draw_bitmap()
                                   ▼
┌────────────────────────────────────────────────────────────────┐
│                       SPI DMA transaction                       │
│                                                                │
│  DMA engine reads LVGL buffer memory                            │
│  streams bytes over SPI MOSI                                    │
│                                                                │
│  Notes:                                                        │
│  - double buffering reduces tearing / improves throughput       │
│  - LINES trades RAM usage vs fewer flush calls                  │
│  - swap_bytes may be required for RGB565 endianness             │
└────────────────────────────────────────────────────────────────┘
```

**Key knobs (via Kconfig):**

* `APP_LVGL_BUF_LINES` → memory use & throughput
* `APP_LVGL_DOUBLE_BUFFER` → latency/tearing vs RAM
* `APP_LVGL_BUFF_DMA` → ensures buffers are DMA-capable
* `APP_LCD_SWAP_BYTES` → fixes RGB565 endianness mismatch
* rotation flags → impacts mapping (mirror_x, swap_xy…)

---

## 3) Display Flush Micro-Flow (What happens per frame)

```
LVGL invalidates an area (dirty rectangle)
         │
         ▼
LVGL renders into draw buffer (RGB565)
         │
         ▼
esp_lvgl_port flush callback
   - optional swap_bytes
   - calls esp_lcd_panel_draw_bitmap(panel, x1,y1,x2,y2, buf)
         │
         ▼
esp_lcd_panel_io_spi queues SPI transactions:
   - set column addr (CASET)
   - set page addr   (PASET)
   - memory write    (RAMWR)
   - stream pixel bytes
         │
         ▼
SPI master + DMA transfers bytes to MOSI
         │
         ▼
ILI9341 GRAM updated → visible pixels
```

### Where tearing/flicker can come from (SPI path)

* insufficient buffer lines (too many flushes)
* too-low SPI clock
* wrong DC timing or transaction queue size
* missing `swap_bytes` and/or wrong RGB/BGR config (visual artifacts)
* display inversion / MADCTL mismatch (mirroring, strange colors)

---

## 4) Touch Read Micro-Flow (and why manual reset mattered)

```
LVGL indev read (poll)
        │
        ▼
esp_lvgl_port reads touch:
   esp_lcd_touch_read_data(tp)
        │
        ▼
FT6x36 driver does I2C reads:
   - read status / points
   - read X/Y registers
        │
        ▼
esp_lcd_touch_get_coordinates()
        │
        ▼
LVGL receives:
   state: PRESSED/RELEASED
   point: x,y
        │
        ▼
LVGL event system:
   - pressed
   - released
   - clicked
   - dragged, etc.
```

### Root cause you fixed

FT6x36 init previously read all registers as `0x00` (vend/chip ID invalid). That typically happens when:

* controller not booted yet (timing)
* held in reset
* brownout / unstable I2C
* double reset sequence

**Manual reset + boot delay + `rst_gpio_num=-1`** ensured:

* you control timing once
* driver doesn’t re-reset unexpectedly
* FT6x36 is alive before ID registers are read

---

## 5) Rotation/Orientation Stack (3 layers, 1 final result)

This matters because you’ve used a combination that works, and it’s easy to break later.

```
┌──────────────────────────────────────────┐
│ (A) Panel memory orientation (MADCTL)    │
│  - ILI9341 register 0x36                 │
│  - controls axis swap/mirror in GRAM     │
└───────────────────┬──────────────────────┘
                    │
┌──────────────────────────────────────────┐
│ (B) esp_lcd panel helpers                │
│  - esp_lcd_panel_mirror()                │
│  - esp_lcd_panel_swap_xy()               │
│  - (may be no-op depending driver impl)  │
└───────────────────┬──────────────────────┘
                    │
┌──────────────────────────────────────────┐
│ (C) LVGL port rotation flags             │
│  - rotation.swap_xy / mirror_x/y         │
│  - affects how LVGL coordinates map      │
│    into the flush drawing                │
└───────────────────┬──────────────────────┘
                    │
                    ▼
Final on-screen orientation
```

**Practical rule:**
Pick **one primary layer** to control orientation (you’re currently using LVGL port rotation + MADCTL tweaks for invert). Document it and don’t “fight” it with multiple layers.

---

## 6) Build-Time Composition (Kconfig + CMake) — “Clean HAL”

```
Kconfig.projbuild
  ├─ board profile defaults
  ├─ selected display driver (ILI9341 SPI now)
  ├─ selected touch driver (FT6x36 now)
  ├─ pins, clocks, resolution
  ├─ LVGL buffer lines, DMA, double buffer
  ├─ rotation flags (int 0/1)
  └─ UI mode selection

          │ generates
          ▼

sdkconfig / sdkconfig.h macros (CONFIG_APP_...)

          │ consumed by
          ▼

main/CMakeLists.txt
  - includes correct HAL source files only
  - avoids dead code and unused modules

          │ builds
          ▼

Firmware with only selected drivers compiled
```

---

## 7) Future ESP32-P4 Path (deep technical view)

### What changes on P4 (typical)

* Display bus likely becomes **RGB parallel** or **MIPI DSI**
* Framebuffer may live in **PSRAM** (or dedicated memory)
* LVGL can run in:

  * **partial buffer mode** (like now), or
  * **full framebuffer mode** (higher memory, simpler flush)
* Potential acceleration (depending on peripherals / GPU support)

### Future display pipeline (conceptual)

```
LVGL render
   │
   ├─ Option 1: Partial buffers (like SPI today)
   │     LVGL buf -> RGB/MIPI driver -> panel
   │
   └─ Option 2: Full framebuffer in PSRAM
         LVGL draws into framebuffer
         display controller scans out framebuffer continuously
         (less tearing, more memory, different sync model)
```

**HAL impact:**
Only `app_display_*` and `app_lvgl` buffer strategy should change. UI and touch should remain unchanged.

