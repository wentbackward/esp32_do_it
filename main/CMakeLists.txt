set(SRCS
    "main.c"
    "app_lvgl.c"
    "ui_hwtest.c"
)

# Display drivers
if(CONFIG_APP_DISPLAY_ILI9341_SPI)
    list(APPEND SRCS "app_display_ili9341.c")
elseif(CONFIG_APP_DISPLAY_RGB_PARALLEL)
    list(APPEND SRCS "app_display_rgb.c")
endif()

# Touch drivers
if(CONFIG_APP_TOUCH_FT6X36_I2C)
    list(APPEND SRCS "app_touch_ft6x36.c")
elseif(CONFIG_APP_TOUCH_GT911_I2C)
    list(APPEND SRCS "app_touch_gt911.c")
endif()

idf_component_register(SRCS ${SRCS} INCLUDE_DIRS ".")
