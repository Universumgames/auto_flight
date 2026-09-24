
if(NOT NATIVE_BUILD)
FILE(GLOB flight_data_srcs "*.cpp")

idf_component_register(
        SRCS ${flight_data_srcs}
        INCLUDE_DIRS "."
        REQUIRES freertos esp_timer gps device_id nlohmann-json
)
else()

idf_component_register(
        SRCS "types.cpp" "serializer.cpp" "geo_helper.cpp"
        INCLUDE_DIRS "." "${NLOHMANN_JSON_INCLUDE_DIR}"
)
endif()