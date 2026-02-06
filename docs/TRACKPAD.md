# USB Trackpad Mode

This document describes the USB HID trackpad implementation that turns the touch display into a USB mouse/trackpad.

## Overview

The trackpad architecture has been refactored to separate the UI visualization from the high-frequency input processing.

- **`app_trackpad.c`**: The core service running a 100Hz FreeRTOS task. It reads the touch hardware, processes gestures, and sends USB HID reports.
- **`ui_trackpad.c`**: The LVGL UI layer. It visualizes the cursor and scroll zones but does **not** process input or send HID reports. It polls the service state at 30Hz.
- **`trackpad_gesture.hpp`**: The gesture engine implementing velocity smoothing, acceleration, and gesture recognition.

## Features

- **Linear Acceleration**: Predictable speed ramp-up with a 1.2x base speed for responsiveness and a 3.5x cap for control.
- **Velocity Smoothing**: Heavy EWMA filtering (0.3/0.7) to reject hardware noise and jitter.
- **Smart Braking**: Damps velocity by 50% on direction changes to prevent "jumps" while avoiding "stutter".
- **Startup Protection**: Clamps excessive coordinate jumps during the first 100ms of a touch to prevent erratic behavior.
- **Tap to Click**: Strict 2px threshold ensures only stationary touches register as clicks.
- **Scroll Zones**: Configurable zones on right/bottom edges.

## Configuration

The trackpad can be configured via **menuconfig**:

```
Component config -> App: Display + Touch + LVGL Template -> HID: Trackpad
```

| Option | Description | Default |
|--------|-------------|---------|
| `APP_HID_TRACKPAD_SCROLL_ENABLE` | Enable/Disable scroll zones | `y` |
| `APP_HID_TRACKPAD_SCROLL_PERCENT` | Size of scroll zones (%) | `10` |

## Architecture

### 1. Input Processing (`app_trackpad.c`)
- **Task**: `trackpad_poll_task`
- **Priority**: 10 (High)
- **Rate**: 100Hz (Matched to hardware)
- **Flow**:
  1. Read Touch (I2C)
  2. Update Shared State (for UI)
  3. Process Gesture (`trackpad_process_input`)
  4. Send HID Report (`app_hid_trackpad_send_...`) with retry logic

### 2. UI Visualization (`ui_trackpad.c`)
- **Timer**: `ui_update_timer_cb`
- **Rate**: 30Hz
- **Flow**:
  1. `app_trackpad_get_status()`
  2. Update LVGL cursor position
  3. Highlight active scroll zones

### 3. Gesture Engine (`trackpad_gesture.hpp`)
- **Input**: Raw X/Y coordinates + Timestamp
- **Processing**:
  - `dt` calculation
  - Delta clamping (startup)
  - EWMA Velocity Smoothing
  - Linear Acceleration Calculation
  - State Machine (Idle -> Moving -> Tap/Drag)
- **Output**: Delta X/Y, Scroll, Clicks

## Tuning

The gesture engine parameters are hardcoded in `trackpad_gesture.hpp` for performance but can be tuned:

| Parameter | Value | Effect |
|-----------|-------|--------|
| `accel_min` | 1.2f | Responsiveness at low speeds (higher = lighter feel) |
| `accel_max` | 3.5f | Top speed for flicks |
| `accel_velocity_scale` | 1200.0f | How fast it reaches top speed (lower = faster ramp) |
| `anti_wiggle_px` | 0 | Deadzone (0 = none) |
| `tap_max_movement_px` | 2 | Max wiggle allowed for a "tap" |

## Troubleshooting

- **Cursor Jumps:** Check `Startup stability filter` in `trackpad_gesture.hpp`.
- **Stuttering:** Check `touch_poll_task` priority and polling rate matching hardware.
- **Sticky / Dropped Inputs:** Check `app_hid_trackpad.c` retry logic (currently 5 retries).