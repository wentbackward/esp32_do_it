/**
 * @file app_hid_gamepad.h
 * @brief Gamepad mode HID implementation (USB Gamepad)
 */

#pragma once

#include "app_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

// Gamepad button bits
#define GAMEPAD_BTN_A  0x01
#define GAMEPAD_BTN_B  0x02
#define GAMEPAD_BTN_X  0x04
#define GAMEPAD_BTN_Y  0x08

// Gamepad mode uses functions defined in app_hid.h:
// - app_hid_init()
// - app_hid_gamepad_send_state()

#ifdef __cplusplus
}
#endif
