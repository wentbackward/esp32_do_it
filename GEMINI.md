# GEMINI.md

## Project Overview

This is an ESP-IDF project for the ESP32 microcontroller, designed as a production-grade template for applications using the LVGL graphics library. The project provides a clean starting point for building embedded user interfaces, with a focus on a configuration-driven workflow and hardware abstraction.

The architecture is layered to separate UI logic, the LVGL platform layer, a hardware abstraction layer (HAL), and the underlying ESP-IDF drivers. This design promotes modularity, maintainability, and makes it easier to adapt the project to different hardware configurations.

The project also includes advanced features like USB HID support (for trackpad, keyboard, and macropad functionality) and a comprehensive testing suite for the trackpad gesture recognition logic.

## Building and Running

### 1. Setup ESP-IDF
Ensure ESP-IDF version 5.x is installed and activated in your shell.
```bash
idf.py --version
```

### 2. Configure the Project
The project is configured using the ESP-IDF menuconfig system. This allows you to set up the display, touch controller, and other application-level settings.
```bash
idf.py menuconfig
```

### 3. Build and Flash
To build the application, flash it to the device, and open a serial monitor, run the following command:
```bash
idf.py build flash monitor
```

## Development Conventions

*   **Configuration-Driven:** The project emphasizes a configuration-first approach using `menuconfig`. Instead of hard-coding display parameters or other settings, you should use the Kconfig options to configure the application.
*   **Hardware Abstraction:** The project uses a Hardware Abstraction Layer (HAL) to separate the application logic from the hardware-specific drivers. The display and touch controllers are abstracted, allowing for easier porting to different hardware.
*   **Testing:** The trackpad gesture recognition feature includes a suite of unit tests that can be run on the host machine for fast development and validation. See `test/README.md` for more information on running the tests.

## Key Files

*   `main/main.c`: The main application entry point. It initializes the display, touch, LVGL, and the selected UI.
*   `main/app_display_*.c`: Hardware-specific display drivers. The correct driver is selected at build time based on the project configuration.
*   `main/app_touch_*.c`: Hardware-specific touch drivers.
*   `main/app_lvgl.c`: LVGL platform layer, responsible for initializing and configuring LVGL.
*   `main/ui_*.c`: UI modules. The active UI is selected at build time.
*   `docs/ARCHITECTURE.md`: A detailed overview of the project's architecture.
*   `Kconfig.projbuild`: Defines the application-level configuration options that are available in `menuconfig`.
*   `sdkconfig.defaults`: Default configuration values for the project.
*   `CMakeLists.txt`: The main CMake file for the project.
*   `README.md`: The main project README file.

## Usage

This project is intended to be used as a template for new ESP32 + LVGL projects. To start a new project based on this template, you can clone the repository and then customize it for your specific needs. The main areas to customize are:

*   **Display and Touch Drivers:** If your hardware is not already supported, you will need to create new display and touch driver files in the `main` directory and add the corresponding Kconfig options.
*   **UI:** The project includes several example UIs. You should replace the example UI with your own application UI. The `main/ui_simple.c` file is a good starting point.
*   **Project Name and Identifiers:** You should rename the project and update the relevant identifiers in the `CMakeLists.txt` file and other project files.
