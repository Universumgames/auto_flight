
if(NOT NATIVE_BUILD)
FILE(GLOB flight_data_srcs "*.cpp")

idf_component_register(
        SRCS ${flight_data_srcs}
        INCLUDE_DIRS "."
        REQUIRES nlohmann-json freertos esp_timer gps
)
else()
# FlightStorage.cpp pulls in FreeRTOS/GPS_Reader and is ESP-only; the rest
# (types/serializer/geo_helper) is plain C++ and needed by native tests.
# nlohmann-json is normally resolved by the ESP-IDF component manager, so
# for native builds point at the vendored copy directly.
idf_component_register(
        SRCS "types.cpp" "serializer.cpp" "geo_helper.cpp"
        INCLUDE_DIRS "." "${CMAKE_SOURCE_DIR}/managed_components/johboh__nlohmann-json/single_include"
)
endif()