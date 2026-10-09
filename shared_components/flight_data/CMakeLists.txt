
if(NOT NATIVE_BUILD)
FILE(GLOB flight_data_srcs "*.cpp")

idf_component_register(
        SRCS ${flight_data_srcs}
        INCLUDE_DIRS "."
        REQUIRES freertos esp_timer gps device_id nlohmann-json flight_com
)
else()

idf_component_register(
        SRCS "serializer.cpp" "geo_helper.cpp"
        INCLUDE_DIRS "." "${NLOHMANN_JSON_INCLUDE_DIR}"
)
endif()