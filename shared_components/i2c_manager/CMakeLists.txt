if(NOT NATIVE_BUILD)
    FILE(GLOB i2c_manager_srcs "*.cpp")

idf_component_register(
        SRCS ${i2c_manager_srcs}
        INCLUDE_DIRS "."
        PRIV_REQUIRES freertos esp_driver_gpio
        REQUIRES esp_driver_i2c i2c_bus
)

endif()