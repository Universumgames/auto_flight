
if(NOT NATIVE_BUILD)
FILE(GLOB accelerometer_srcs "*.cpp")

idf_component_register(
        SRCS ${accelerometer_srcs}
        INCLUDE_DIRS "."
        REQUIRES mpu6050 esp_timer freertos i2c_manager
)
endif()