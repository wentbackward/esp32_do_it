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

