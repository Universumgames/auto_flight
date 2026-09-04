FILE(GLOB base_control_srcs "*.cpp")

idf_component_register(
        SRCS ${base_control_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_http_server flight_data gps barometer battery lora_com i2c_manager flight_com oled_display frontend_bluetooth
)