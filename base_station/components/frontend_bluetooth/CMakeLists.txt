FILE(GLOB frontend_bl_srcs "*.cpp")

idf_component_register(
        SRCS ${frontend_bl_srcs}
        INCLUDE_DIRS "."
        REQUIRES bt nvs_flash esp_driver_uart freertos flight_data barometer
)