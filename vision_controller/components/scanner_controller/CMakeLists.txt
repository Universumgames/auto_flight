if (NOT NATIVE_BUILD)
    FILE(GLOB scanner_controller_srcs "*.cpp")

    idf_component_register(
            SRCS ${scanner_controller_srcs}
            INCLUDE_DIRS "."
            PRIV_REQUIRES esp_driver_gpio
            REQUIRES freertos esp_timer esp_driver_i2c i2c_bus flight_com cnn camera cam_com
    )
endif ()
