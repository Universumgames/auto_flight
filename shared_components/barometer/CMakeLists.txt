
if(NOT NATIVE_BUILD)
FILE(GLOB barometer_srcs "*.cpp")

idf_component_register(
        SRCS ${barometer_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_timer freertos
        PRIV_REQUIRES i2c_manager bme280
)
endif()