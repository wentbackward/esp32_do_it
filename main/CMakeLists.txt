set(SRCS
    "main.c"
    "app_display_ili9341.c"
    "app_lvgl.c"
    "ui_hwtest.c"
)

if(CONFIG_APP_TOUCH_FT6X36_I2C)
    list(APPEND SRCS "app_touch_ft6x36.c")
endif()

idf_component_register(SRCS ${SRCS} INCLUDE_DIRS ".")
