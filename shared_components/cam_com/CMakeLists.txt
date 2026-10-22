FILE(GLOB camera_com_srcs "*.cpp")

idf_component_register(
        SRCS ${camera_com_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_driver_i2c i2c_manager freertos flight_com
)