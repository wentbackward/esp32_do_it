idf_component_register(SRCS "hello_world_main.c" "ui_hwtest.c"
                       PRIV_REQUIRES spi_flash esp_lcd esp_lcd_touch esp_lvgl_port
                       INCLUDE_DIRS ".")
