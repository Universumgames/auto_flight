
if(NOT NATIVE_BUILD)
FILE(GLOB flight_data_srcs "*.cpp")

idf_component_register(
        SRCS ${flight_data_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_http_server nlohmann-json driver freertos esp_timer gps
)
endif()