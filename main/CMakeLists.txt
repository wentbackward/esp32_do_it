set(SRCS
    "main.c"
    "app_lvgl.c"
    "ui_hwtest.c"
    "hw_display_test.c"
)

# Display drivers
if(CONFIG_APP_DISPLAY_ILI9341_SPI)
    list(APPEND SRCS "app_display_ili9341.c")
elseif(CONFIG_APP_DISPLAY_RGB_PARALLEL)
    list(APPEND SRCS "app_display_rgb.c")
elseif(CONFIG_APP_DISPLAY_LGFX)
    list(APPEND SRCS "app_display_lgfx.cpp")
endif()

# Touch drivers
if(CONFIG_APP_TOUCH_FT6X36_I2C)
    list(APPEND SRCS "app_touch_ft6x36.c")
elseif(CONFIG_APP_TOUCH_GT911_I2C)
    list(APPEND SRCS "app_touch_gt911.c")
endif()

# HID drivers (build-time selection)
if(CONFIG_APP_HID_MODE_TRACKPAD)
    list(APPEND SRCS "trackpad_gesture.cpp" "app_hid_trackpad.c" "ui_trackpad.c")
elseif(CONFIG_APP_HID_MODE_MACROPAD)
    list(APPEND SRCS "app_hid_macropad.c" "ui_macropad.c")
elseif(CONFIG_APP_HID_MODE_GAMEPAD)
    list(APPEND SRCS "app_hid_gamepad.c" "ui_gamepad.c")
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
)
