FILE(GLOB cam_com_srcs "*.cpp")

idf_component_register(
        SRCS ${cam_com_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_driver_i2c cam_com i2c_manager freertos flight_com
)