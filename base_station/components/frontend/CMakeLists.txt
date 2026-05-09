FILE(GLOB frontend_srcs "*.cpp")

idf_component_register(
        SRCS ${frontend_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_http_server littlefs
)