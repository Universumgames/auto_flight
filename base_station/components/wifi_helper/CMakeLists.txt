FILE(GLOB wifi_helper_srcs "*.cpp")

set(SECRETS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/secrets.h")

if(EXISTS ${SECRETS_FILE})
    add_compile_definitions(HAVE_WIFI_SECRETS_H=1)
endif()

idf_component_register(
        SRCS ${wifi_helper_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_wifi nvs_flash
)