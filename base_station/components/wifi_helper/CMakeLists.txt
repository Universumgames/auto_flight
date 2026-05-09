FILE(GLOB wifi_helper_srcs "*.cpp")

idf_component_register(
        SRCS ${wifi_helper_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_wifi nvs_flash
)