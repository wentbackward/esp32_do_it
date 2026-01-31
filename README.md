# ESP32 + LVGL Template  
*A production-grade starting point for ESP-IDF & LVGL 9.4 projects*

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-blue)
![LVGL](https://img.shields.io/badge/LVGL-9.4-purple)
![Platform](https://img.shields.io/badge/platform-ESP32-green)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

---

## What This Is

This repository is a **GitHub template** designed to get you from  
**clone → pixels on screen** as quickly and cleanly as possible.

It provides a:
- Known-good ESP-IDF application structure
- Correct, modern LVGL 9.4 integration
- Configuration-first workflow using `menuconfig`
- Hardware-agnostic display & touch abstraction

Use it as the **foundation** for real products, prototypes, or experiments.

---

## Why Use This Template?

ESP32 + LVGL projects often stall on:
- Display initialization edge cases
- Rotation, mirroring, and touch alignment
- LVGL timing & tick handling
- ESP-IDF component wiring and Kconfig sprawl

This template removes those problems **up front**, so you can focus on:
- UI design
- Product logic
- Performance tuning
- Shipping real hardware

---

## Philosophy: Configuration Over Abstraction

This project takes a different path than Arduino or PlatformIO.

**The Problem with Universal Platforms:**
Platforms like Arduino and PlatformIO try to accommodate all boards, creating abstraction layers that eventually become obstacles when you need to customize, optimize, or upgrade.

**Why ESP-IDF Direct:**
- **Modern Versions**: Most ESP32 examples online use outdated dependencies. LVGL is stuck on v8 in most platforms; this project uses **v9.4+** and can quickly adopt new versions.
- **Rapid Upgrades**: When ESP-IDF 6 exits beta, we upgrade immediately—no waiting for platform maintainers. New hardware accelerations and optimizations arrive faster.
- **Board-Specific Reality**: ESP32 board availability and peripheral combinations are increasingly diverse. Boards change between iterations. Instead of fighting abstractions, we use **Kconfig** to embrace board knowledge upfront.
- **No Delayed Friction**: In embedded development, you *will* need to understand your hardware. This project acknowledges that from day one, reducing friction rather than deferring it.

**Compatibility Without Constraint:**
You can still use Arduino or PlatformIO if needed, but this project doesn't let their limitations constrain your foundation.

This approach keeps the latest ESP-IDF, LVGL, and hardware changes accessible without platform bloat.

---

## Quick Start (10 Minutes)

### 1. Create Your Project
Click **“Use this template”** on GitHub, or clone directly:

```bash
git clone https://github.com/wentbackward/esp32_do_it.git
cd esp32_do_it
````

### 2. Set Up ESP-IDF

Ensure ESP-IDF 5.x is installed and active:

```bash
idf.py --version
```

### 3. Configure the Application

```bash
idf.py menuconfig
```

Key sections:

* **Application Configuration**
* **LVGL Configuration**
* **Display Configuration**
* **Touch Configuration (optional)**

### 4. Build & Flash

```bash
idf.py build flash monitor
```

If your display parameters are correct, you should see LVGL output immediately.

---

## Features

This project includes production-ready implementations beyond the basic template:

### 🖱️ **USB HID Support**
Complete USB HID implementation for multiple device types:

- **Trackpad Mode**: Touch display acts as USB mouse/trackpad
  - Pointer movement with dual-phase acceleration
  - Tap-to-click with swipe cancellation
  - Click-and-drag (tap-tap-drag gesture)
  - Edge scroll zones (vertical/horizontal)
  - Comprehensive gesture recognition
  - 📖 See [`docs/TRACKPAD.md`](docs/TRACKPAD.md) for full documentation

- **Keyboard Mode**: Custom keyboard layouts and macros
- **Macropad Mode**: Programmable macro keys

**Documentation:**
- [`docs/TRACKPAD.md`](docs/TRACKPAD.md) - Trackpad feature guide
- [`docs/TRACKPAD_TESTING.md`](docs/TRACKPAD_TESTING.md) - Test architecture
- [`docs/TRACKPAD_INTEGRATION_GUIDE.md`](docs/TRACKPAD_INTEGRATION_GUIDE.md) - Integration guide

### 🧪 **Comprehensive Testing**
The trackpad gesture recognition includes 47+ automated unit tests:
- Pure function tests (no hardware needed)
- Gesture sequence tests (tap, drag, scroll)
- Host-compilable tests for fast iteration
- See [`test/README.md`](test/README.md) for running tests

### 🎨 **Hardware Test UI**
LVGL-based diagnostics UI with:
- Auto-calculated proportional grid
- Touch coordinate display
- Color accuracy swatches
- Orientation verification
- FPS counter
- Backlight/invert controls (when supported)

---

## Project Structure

```text
.
├── main/
│   ├── app_main.c        # Application entry point
│   ├── app_display.c     # Display + LVGL binding
│   ├── app_touch.c       # Touch abstraction (optional)
│   └── app_ui.c          # UI bootstrap (demo or custom)
│
├── components/
│   └── app_lvgl/         # LVGL helpers & glue code
│
├── Kconfig.projbuild     # App-level menuconfig options
├── sdkconfig.defaults   # Sensible defaults
└── README.md
```

Each layer has a single responsibility:

* **Hardware setup** is isolated
* **UI logic** is cleanly separated
* **Configuration** is explicit and discoverable

---

## Configuration Philosophy

This template is **configuration-driven**.

You should not need to:

* Edit core C files for rotation or mirroring
* Hard-code display parameters
* Comment/uncomment logic to test options

Instead, use `menuconfig` to:

* Enable or disable touch
* Adjust orientation & axis swapping
* Select LVGL demos
* Tune memory and buffer sizes

---

## LVGL Demos (Optional)

LVGL’s built-in demos can be enabled via `menuconfig`.

They are useful for:

* Verifying display performance
* Testing touch accuracy
* Stress-testing memory and refresh paths

Once validated, disable them and plug in your own UI.

---

## Performance by Design

The template is structured so performance tuning is:

* Incremental
* Observable
* Non-disruptive

It supports:

* Double buffering
* DMA-friendly display flushing
* Clean LVGL task scheduling
* Easy instrumentation and logging

You can start simple and optimize later without rewrites.

---

## What You’re Expected to Customize

After creating your own repository from this template, you should:

* Replace or configure the display driver for your panel
* Adjust resolution and color depth
* Swap the demo UI for your application UI
* Rename the project and identifiers

See `app_ui.c` for the intended UI entry point.

---

## What This Template Does *Not* Do

This repository intentionally avoids:

* Board-specific assumptions
* Opinionated UI frameworks
* Hidden or “magic” abstractions
* Vendor lock-in

It gives you a **solid floor**, not a low ceiling.

---

## Who This Is For

* ESP-IDF newcomers who want a reliable start
* Experienced developers tired of boilerplate
* Teams building real ESP32 UI products
* Anyone who values clarity, structure, and maintainability

---

## 📜 License

MIT — use it, fork it, ship it.

---

## Final Note

Embedded UI work is hard enough.

This template exists so you don’t have to earn the right to draw your first pixel.

Happy building. 

