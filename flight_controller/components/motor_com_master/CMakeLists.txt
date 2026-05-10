FILE(GLOB motor_com_srcs "*.cpp")

idf_component_register(
        SRCS ${motor_com_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_driver_i2c motor_com
)