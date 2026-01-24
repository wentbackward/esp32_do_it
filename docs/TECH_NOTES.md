# AI PROJECT CONTEXT PROMPT

## 1) Project Overview

**Name:**
ESP32 LVGL Display & Touch Platform (Modular HAL Template)

**Purpose:**
Provide a reusable, configurable embedded GUI platform for ESP32 devices (starting with ESP32-S3), supporting displays, touch controllers, and LVGL with a clean hardware abstraction layer and Kconfig-driven configuration.

**Primary use cases:**

* Rapid prototyping of embedded GUIs
* Hardware bring-up for new display + touch combinations
* Foundation for future ESP32-P4 RGB/MIPI display projects
* Reusable template for embedded UI firmware

**Target users:**

* Embedded developers working with ESP32 and LVGL
* Hardware engineers integrating displays and touch controllers
* Future expansion to production-grade embedded devices

---

## 2) System Architecture

### High-level architecture

The system is structured as layered modules:

```
Application / UI Layer
 ├─ ui_hwtest.c
 ├─ ui_simple.c
 └─ LVGL demos

LVGL Platform Layer
 └─ app_lvgl.c
      (LVGL port integration, buffers, rotation, DMA flags)

Hardware Abstraction Layer (HAL)
 ├─ Display HAL
 │   └─ app_display_ili9341.c (SPI ILI9341)
 └─ Touch HAL
     └─ app_touch_ft6x36.c (I2C FT6x36/FT6336)

Board Configuration Layer
 └─ Kconfig.projbuild (pins, clocks, drivers, rotation, UI mode)

ESP-IDF / esp_lcd / LVGL / Drivers
```

### Data flow

1. ESP-IDF boots → `app_main()`
2. Display HAL initializes panel and SPI bus
3. Touch HAL initializes I2C and FT6x36 controller
4. LVGL port is initialized and connected to display + touch
5. Selected UI mode is launched (hwtest, simple UI, or demo)
6. LVGL task renders UI and handles input events

### Key design patterns / paradigms

* Hardware Abstraction Layer (HAL)
* Driver selection via Kconfig + CMake
* Build-time feature composition (not runtime flags)
* Modular separation of display, touch, LVGL, UI
* Board profile defaults via Kconfig
* Defensive embedded initialization (manual reset, timing control)

### Runtime environment

* Platform: ESP32-S3 (current), ESP32-P4 planned
* OS: FreeRTOS (via ESP-IDF)
* Graphics: LVGL 9.x
* Drivers: esp_lcd, esp_lvgl_port, custom HAL wrappers

---

## 3) Tech Stack

**Languages:**

* C (ESP-IDF, LVGL)

**Frameworks:**

* ESP-IDF (v5.x)
* LVGL 9.4
* esp_lcd / esp_lvgl_port

**Libraries / Components:**

* esp_lcd_ili9341
* esp_lcd_touch_ft6x36
* lvgl/lvgl
* espressif/esp_lvgl_port

**Infrastructure / Cloud:**

* None (embedded firmware)

**Databases / Storage:**

* None (optional future NVS usage)

**External APIs / Integrations:**

* None

**Tooling:**

* ESP-IDF build system (idf.py)
* Kconfig / menuconfig
* CMake
* Managed components (idf_component.yml)
* Serial flashing/debugging tools

---

## 4) Repository Structure

**Key directories and responsibilities:**

```
main/
 ├─ main.c                 # Application entry point
 ├─ app_display_ili9341.c  # Display HAL (ILI9341 SPI)
 ├─ app_display_ili9341.h
 ├─ app_touch_ft6x36.c     # Touch HAL (FT6x36 I2C)
 ├─ app_touch_ft6x36.h
 ├─ app_lvgl.c             # LVGL integration layer
 ├─ app_lvgl.h
 ├─ ui_hwtest.c            # Hardware test UI
 ├─ ui_hwtest.h
 ├─ Kconfig.projbuild      # Project-specific Kconfig
 └─ CMakeLists.txt         # Component build configuration

idf_component.yml          # Managed dependencies
sdkconfig.defaults         # Default configuration
```

**Entry point:**

* `app_main()` in `main.c`

**Configuration files:**

* `main/Kconfig.projbuild`
* `sdkconfig`
* `sdkconfig.defaults`
* `idf_component.yml`

---

## 5) Current Implementation Status

### Implemented features

* SPI ILI9341 display driver (esp_lcd)
* FT6x36/FT6336 capacitive touch driver (esp_lcd_touch)
* Manual touch reset sequence for stability
* LVGL 9.4 integration via esp_lvgl_port
* Modular HAL for display and touch
* Kconfig-driven configuration (pins, clocks, rotation, UI mode)
* Board profile system (ESP32-S3 baseline)
* Hardware test UI (colors, orientation, buttons, diagnostics)
* LVGL demo integration
* Clean build-time driver selection via CMake

### Partially implemented features

* Touch INT pin support (planned but disabled by default)
* Backlight PWM control (placeholder)
* Multiple display driver support (RGB/MIPI planned)
* Multiple touch driver support (GT911 planned)
* Board profile expansion for ESP32-P4

### Planned features

* RGB parallel / MIPI display HAL for ESP32-P4
* GT911 touch driver HAL
* Advanced hardware test suite (FPS, DMA stress, touch heatmap)
* Calibration tools (touch + color)
* Unified driver registry layer
* PSRAM framebuffer optimization for P4

### Known working flows

* Boot → display init → touch init → LVGL → UI
* Touch interaction with LVGL widgets
* Kconfig-driven UI mode switching
* Stable FT6x36 initialization using manual reset

---

## 6) Key Technical Decisions

### Decisions made

* Use HAL abstraction for display and touch
* Use Kconfig + CMake for driver selection instead of runtime flags
* Manual FT6x36 reset to avoid vendor ID failures
* LVGL port configuration centralized in `app_lvgl.c`
* Rotation flags represented as `int` in Kconfig (not bool)
* Avoid double touch registration in LVGL
* Modular file structure for future ESP32-P4 migration

### Rationale

* Embedded GUI projects require hardware flexibility
* ESP32-P4 will fundamentally change display architecture (RGB/MIPI)
* Kconfig provides reproducible board configurations
* Manual reset solves real-world FT6x36 timing issues
* Modular design reduces coupling between UI and hardware

### Trade-offs

* Slightly more complex build system
* More files/modules than a monolithic firmware
* Build-time configuration instead of runtime flexibility

### Rejected alternatives

* Monolithic `main.c`
* Hardcoded pins and display parameters
* Runtime driver selection
* Relying on FT6x36 automatic reset timing
* Using LVGL config without ESP-IDF Kconfig integration

---

## 7) Requirements & Constraints

### Functional requirements

* Support SPI ILI9341 display
* Support FT6x36 capacitive touch
* Run LVGL 9.x reliably
* Provide configurable board parameters
* Support multiple UI modes
* Enable future display/touch expansion

### Non-functional requirements

* Stability on boot (no random touch init failures)
* Predictable configuration via Kconfig
* Maintainable modular architecture
* Future scalability to ESP32-P4
* Reasonable performance (SPI throughput, LVGL FPS)

### Hard constraints

* ESP-IDF framework
* ESP32 hardware platform
* LVGL 9.x API
* esp_lcd driver ecosystem
* Embedded memory limitations

---

## 8) Known Issues & Technical Debt

### Bugs

* FT6x36 INT pin behavior inconsistent across boards
* I2C driver uses legacy ESP-IDF API (migration pending)

### Limitations

* Only SPI ILI9341 fully implemented
* Touch HAL only FT6x36 currently
* No runtime calibration UI yet
* No unified driver registry layer

### Risks

* ESP32-P4 RGB/MIPI integration complexity
* LVGL memory usage on high-resolution displays
* Touch controller variability across vendors
* Kconfig complexity growth

### TODOs

* Add GT911 touch HAL
* Add RGB/MIPI display HAL
* Implement backlight PWM control
* Add performance diagnostics
* Add touch calibration framework
* Formalize driver interface layer

---

## 9) Open Questions

* Should INT-based touch gating be enabled by default once stable?
* Should display drivers share a common virtual interface?
* Should LVGL buffer strategy change for ESP32-P4 + PSRAM?
* How many board profiles should be supported long-term?
* Should touch calibration be persistent (NVS)?

---

## 10) Development Guidelines

### Coding standards

* Modular C files with clear HAL boundaries
* Minimal logic in `main.c`
* Prefer explicit initialization over implicit behavior
* Avoid hidden side effects in drivers
* Use ESP-IDF error macros (`ESP_RETURN_ON_ERROR`, etc.)

### Architectural principles

* HAL separation: UI ≠ LVGL ≠ Hardware
* Build-time configuration over runtime branching
* Board profiles define defaults, not logic
* Prepare for RGB/MIPI and PSRAM early

### Testing strategy

* Hardware test UI as primary validation tool
* I2C scan and touch diagnostics during init
* Visual color/orientation tests
* Manual regression testing on real hardware

### Branching / workflow rules

* Prefer incremental changes
* Preserve working hardware behavior
* Avoid refactors without clear justification
* Validate on real hardware after HAL changes

---

## 11) Immediate Next Steps

### High priority

1. Finalize HAL interfaces for display and touch
2. Add driver registry abstraction (display + touch)
3. Extend Kconfig to fully cover ESP32-P4 display paths
4. Add optional INT-based touch mode behind Kconfig flag

### Medium priority

5. Implement RGB display HAL placeholder for P4
6. Implement GT911 touch HAL placeholder
7. Add backlight PWM control
8. Add LVGL performance metrics (FPS, render time)

### Low priority / strategic

9. Add calibration UI and persistence
10. Add automated hardware self-test suite
11. Create documentation for adding new boards

---

## 12) AI Operating Instructions

When working in this repo:

* Do not change architecture without justification.
* Prefer minimal, incremental changes.
* Preserve existing patterns unless explicitly instructed.
* Explain non-trivial design choices.
* Highlight risks and side effects.
* Ask clarifying questions only when necessary.

---

## 13) Compact AI Context (for fast startup)

**ESP32 LVGL Display Platform** is an embedded GUI firmware template built on ESP-IDF and LVGL 9.x.
It uses a modular HAL architecture with separate display and touch drivers configured via Kconfig and CMake.

Core components:

* Display HAL (ILI9341 SPI)
* Touch HAL (FT6x36 I2C)
* LVGL integration layer
* Hardware test and demo UIs

Current state:

* Display and touch are stable on ESP32-S3.
* Kconfig-driven configuration and board profiles are implemented.
* Architecture is prepared for ESP32-P4 RGB/MIPI expansion.

Key constraints:

* ESP-IDF ecosystem, embedded memory limits, hardware variability.

Priority goals:

* Finalize HAL abstraction, prepare for ESP32-P4, expand driver support safely.

Your objective: continue development safely and efficiently.

