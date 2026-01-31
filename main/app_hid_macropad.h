/**
 * @file app_hid_macropad.h
 * @brief Macropad mode HID implementation (USB Keyboard)
 */

#pragma once

#include "app_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

// HID keyboard modifier bits
#define HID_MOD_NONE       0x00
#define HID_MOD_LEFT_CTRL  0x01
#define HID_MOD_LEFT_SHIFT 0x02
#define HID_MOD_LEFT_ALT   0x04
#define HID_MOD_LEFT_GUI   0x08
#define HID_MOD_RIGHT_CTRL 0x10
#define HID_MOD_RIGHT_SHIFT 0x20
#define HID_MOD_RIGHT_ALT  0x40
#define HID_MOD_RIGHT_GUI  0x80

// Common HID keycodes
#define HID_KEY_0          0x27
#define HID_KEY_1          0x1E
#define HID_KEY_2          0x1F
#define HID_KEY_3          0x20
#define HID_KEY_4          0x21
#define HID_KEY_5          0x22
#define HID_KEY_6          0x23
#define HID_KEY_7          0x24
#define HID_KEY_8          0x25
#define HID_KEY_9          0x26

// Macropad mode uses functions defined in app_hid.h:
// - app_hid_init()
// - app_hid_macropad_send_key()
// - app_hid_macropad_release_all()
// - app_hid_macropad_load_mapping()
// - app_hid_macropad_save_mapping()

#ifdef __cplusplus
}
#endif
