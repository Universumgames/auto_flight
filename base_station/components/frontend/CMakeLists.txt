FILE(GLOB frontend_srcs "*.cpp")

idf_component_register(
        SRCS ${frontend_srcs}
        INCLUDE_DIRS "."
        REQUIRES flight_data
        PRIV_REQUIRES barometer gps esp_http_server littlefs
)