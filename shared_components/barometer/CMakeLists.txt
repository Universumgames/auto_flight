
if(NOT NATIVE_BUILD)
FILE(GLOB barometer_srcs "*.cpp")

idf_component_register(
        SRCS ${barometer_srcs}
        INCLUDE_DIRS "."
        REQUIRES bme280 esp_timer freertos i2c_manager
)
endif()