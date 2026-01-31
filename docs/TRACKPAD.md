# USB Trackpad Mode

This document describes the USB HID trackpad implementation that turns the touch display into a USB mouse/trackpad.

## Overview

The trackpad UI (`ui_trackpad.c`) converts touch input into USB HID mouse reports. It supports:

- **Pointer movement** with logarithmic acceleration
- **Tap to click** with swipe cancellation
- **Click and drag** (hold + move)
- **Scroll zones** on screen edges
- **Mode switch button** to toggle to other UI modes

## Screen Layout

```
+----------------------------------+------+
|                                  |      |
|           Mode Button            |  V   |
|              [KB]                |  S   |
|                                  |  c   |
|                                  |  r   |
|        Main Trackpad Area        |  o   |
|                                  |  l   |
|             Version              |  l   |
|                                  |      |
+----------------------------------+------+
|        Horizontal Scroll         |corner|
+----------------------------------+------+
```

- **Main trackpad area**: Pointer movement
- **Right edge**: Vertical scroll zone
- **Bottom edge**: Horizontal scroll zone
- **Top-left**: Mode switch button (if callback configured)
- **Top-right**: Version display

## Gesture Recognition

### Tap to Click

A quick touch-and-release sends a left mouse click.

| Parameter | Value | Description |
|-----------|-------|-------------|
| `TAP_THRESHOLD_MS` | 200ms | Max touch duration for tap |
| `TAP_MOVE_THRESHOLD` | 25px | Max **net displacement** during tap |
| `TAP_JITTER_RATIO` | 2.0 | Jitter detection threshold |

**Improved tap detection:**
- Uses **net displacement** from start point (not cumulative movement)
- Tolerates natural finger jitter during tap
- If cumulative movement is high but net displacement is low, it's recognized as jitter (tap still registers)

Example: You can wiggle your finger slightly while tapping and it will still click.

### Pointer Movement

Dragging your finger moves the mouse pointer. Movement uses **dual-phase acceleration** (industry-standard approach):

- **Precision zone** (< 100 px/s): Sub-unity multiplier (0.5x) for fine control
- **Transition zone** (100-400 px/s): Linear ramp from 0.5x to 1.0x
- **Acceleration zone** (400-1500 px/s): Smooth curve up to 5x
- **Maximum** (> 1500 px/s): 5x acceleration for quick traversal

**Advanced features:**
- **Jitter filtering**: 2px dead zone eliminates cursor vibration when finger is stationary
- **Velocity smoothing**: EWMA (Exponentially Weighted Moving Average) prevents twitchy behavior
- **Touch noise rejection**: Filters out touch panel noise automatically

| Parameter | Value | Description |
|-----------|-------|-------------|
| `JITTER_THRESHOLD` | 2px | Dead zone for noise filtering |
| `VELOCITY_ALPHA` | 0.3 | EWMA smoothing factor |
| `ACCEL_PRECISION_SENSITIVITY` | 0.5 | Sub-unity for fine control |
| `ACCEL_BASE_SENSITIVITY` | 1.0 | Normal speed multiplier |
| `ACCEL_MAX_MULTIPLIER` | 5.0 | Maximum acceleration |
| `ACCEL_PRECISION_THRESHOLD` | 100 px/s | Precision zone limit |
| `ACCEL_LINEAR_THRESHOLD` | 400 px/s | Transition zone limit |
| `ACCEL_MAX_THRESHOLD` | 1500 px/s | Max acceleration threshold |

### Click and Drag (Tap-Tap-Drag)

To drag, use the tap-tap-drag gesture (like laptop trackpads):

1. **First tap**: Quick touch and release (registers as a click)
2. **Second tap + hold**: Touch again within 450ms - **green circle appears**
3. **Move**: Start moving while holding - drag begins
4. **Release**: Lift finger to drop

| Parameter | Value | Description |
|-----------|-------|-------------|
| `DOUBLE_TAP_WINDOW_MS` | 450ms | Max time between first tap and second touch |
| `DRAG_MOVE_THRESHOLD` | 8px | Movement needed to start drag after tap-tap |

**Visual feedback:** A green circle indicator appears when drag mode is ready (after second tap), giving clear feedback that you can now drag.

If you tap twice quickly without moving, it sends a **double-click** instead.

**Gesture summary:**
- Tap → Click
- Tap + Tap (no movement) → Double-click
- Tap + Tap-and-move (green circle shown) → Drag

### Scrolling

Touch the scroll zones to scroll instead of move the pointer.

**Vertical scroll**: Right edge of screen
**Horizontal scroll**: Bottom edge of screen

| Parameter | Value | Description |
|-----------|-------|-------------|
| `SCROLL_ZONE_PERCENT` | 15% | Zone size as percentage of screen |
| `SCROLL_ZONE_MIN_PX` | 30px | Minimum zone width |
| `SCROLL_ZONE_MAX_PX` | 60px | Maximum zone width |
| `SCROLL_SENSITIVITY` | 20px | Pixels per scroll unit |

Scroll zones are clamped between 30-60 pixels regardless of screen size.

## Configuration

### Kconfig Options

```
CONFIG_APP_HID_TRACKPAD_TAP_THRESHOLD_MS  # Tap duration threshold (default: 200)
CONFIG_APP_HID_TRACKPAD_SENSITIVITY       # Base sensitivity multiplier
```

### Compile-Time Constants

All timing and threshold constants are defined at the top of `ui_trackpad.c`:

```c
// Jitter filtering
#define JITTER_THRESHOLD 2

// Velocity smoothing
#define VELOCITY_ALPHA 0.3f

// Acceleration (dual-phase curve)
#define ACCEL_PRECISION_SENSITIVITY 0.5f
#define ACCEL_BASE_SENSITIVITY 1.0f
#define ACCEL_MAX_MULTIPLIER 5.0f
#define ACCEL_PRECISION_THRESHOLD 100.0f   // px/s
#define ACCEL_LINEAR_THRESHOLD 400.0f      // px/s
#define ACCEL_MAX_THRESHOLD 1500.0f        // px/s

// Tap detection
#define TAP_THRESHOLD_MS 200
#define TAP_MOVE_THRESHOLD 25
#define TAP_JITTER_RATIO 2.0f

// Tap-tap-drag
#define DOUBLE_TAP_WINDOW_MS 450
#define DRAG_MOVE_THRESHOLD 8

// Scroll zones
#define SCROLL_ZONE_PERCENT 15
#define SCROLL_ZONE_MIN_PX 30
#define SCROLL_ZONE_MAX_PX 60
#define SCROLL_SENSITIVITY 20
```

## HID Interface

The trackpad uses these HID functions from `app_hid.h`:

```c
// Send pointer movement (with acceleration applied by UI)
esp_err_t app_hid_trackpad_send_move(app_hid_t *hid, int16_t dx, int16_t dy);

// Send button state (0x01 = left, 0x02 = right, 0x04 = middle)
esp_err_t app_hid_trackpad_send_click(app_hid_t *hid, uint8_t buttons);

// Send scroll (vertical and horizontal)
esp_err_t app_hid_trackpad_send_scroll(app_hid_t *hid, int8_t vertical, int8_t horizontal);

// Send combined report (buttons + movement + scroll)
esp_err_t app_hid_trackpad_send_report(app_hid_t *hid, uint8_t buttons,
                                        int16_t dx, int16_t dy,
                                        int8_t scroll_v, int8_t scroll_h);
```

## Mode Switching

The trackpad UI supports a mode switch callback to toggle between UIs:

```c
typedef void (*ui_trackpad_mode_switch_cb_t)(void);

typedef struct {
    uint16_t hres;
    uint16_t vres;
    app_hid_t *hid;
    ui_trackpad_mode_switch_cb_t mode_switch_cb;  // Optional
} trackpad_cfg_t;
```

If `mode_switch_cb` is non-NULL, a keyboard icon button appears in the top-left corner. Pressing it calls the callback, which can switch to macropad or other UI modes.

## Tuning Tips

**Cursor vibrates when finger is stationary:**
- Increase `JITTER_THRESHOLD` (2→3 or 4)

**Cursor feels twitchy/jittery during movement:**
- Decrease `VELOCITY_ALPHA` (0.3→0.2 for more smoothing)
- Increase `JITTER_THRESHOLD`

**Can't position cursor precisely:**
- Increase `ACCEL_PRECISION_SENSITIVITY` (0.5→0.6 or 0.7)
- Increase `ACCEL_PRECISION_THRESHOLD` (100→150 px/s)

**Pointer moves too slow for traversal:**
- Increase `ACCEL_MAX_MULTIPLIER` (5.0→6.0)
- Decrease `ACCEL_MAX_THRESHOLD` (1500→1200 px/s)

**Accidental drags:**
- Decrease `DOUBLE_TAP_WINDOW_MS` (450→350ms)
- Increase `DRAG_MOVE_THRESHOLD` (8→12px)

**Taps not registering:**
- Increase `TAP_THRESHOLD_MS` (200→250ms)
- Increase `TAP_MOVE_THRESHOLD` (25→30px)

**Scroll zones too small/large:**
- Adjust `SCROLL_ZONE_MIN_PX` and `SCROLL_ZONE_MAX_PX`

## Files

| File | Description |
|------|-------------|
| `main/ui_trackpad.c` | Trackpad UI implementation |
| `main/ui_trackpad.h` | Public interface and config struct |
| `main/app_hid_trackpad.c` | USB HID mouse implementation |
| `main/app_hid_trackpad.h` | HID trackpad header |
| `main/app_hid.h` | Common HID interface |
