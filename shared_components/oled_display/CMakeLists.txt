if (NOT NATIVE_BUILD)
    FILE(GLOB oled_display_srcs "*.cpp")

    idf_component_register(
            SRCS ${oled_display_srcs}
            INCLUDE_DIRS "."
            PRIV_REQUIRES esp_driver_gpio flight_data
            REQUIRES esp_driver_i2c esp_lcd esp_oled_ssd1315 lvgl esp_timer
    )
endif ()