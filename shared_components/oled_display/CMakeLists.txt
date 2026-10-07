if (NOT NATIVE_BUILD)
    FILE(GLOB oled_display_srcs "*.cpp")

    idf_component_register(
            SRCS ${oled_display_srcs}
            INCLUDE_DIRS "."
            PRIV_REQUIRES esp_driver_gpio
            REQUIRES esp_driver_i2c esp_lcd esp_oled_ssd1315 lvgl esp_timer flight_com device_id
    )

    # REQUIRES is evaluated in a separate CMake process that does not see project variables,
    # so device-specific dependencies have to be added here, after registration.
    if (FLIGHT_DEVICE_TYPE_BASE_STATION)
        idf_component_optional_requires(PRIVATE proxy_cache)
    elseif (FLIGHT_DEVICE_TYPE_PLANE)
        idf_component_optional_requires(PRIVATE flight_data)
    endif ()
endif ()