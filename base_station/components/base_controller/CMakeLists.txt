FILE(GLOB base_control_srcs "*.cpp")

idf_component_register(
        SRCS ${base_control_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_http_server littlefs flight_data gps frontend barometer lora_com i2c_manager flight_com
)