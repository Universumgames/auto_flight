
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
# nlohmann-json is normally resolved by the ESP-IDF component manager, whose
# managed_components/ output is gitignored and only present once idf.py has
# run. Native builds (e.g. CI) don't run idf.py, so fall back to the system
# package (apt install nlohmann-json3-dev) if the vendored copy isn't there.
find_path(NLOHMANN_JSON_INCLUDE_DIR "nlohmann/json.hpp"
        PATHS "${CMAKE_SOURCE_DIR}/managed_components/johboh__nlohmann-json/single_include"
        NO_DEFAULT_PATH
)

if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    find_path(NLOHMANN_JSON_INCLUDE_DIR "nlohmann/json.hpp")
endif()

if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    message(FATAL_ERROR "nlohmann/json.hpp not found: run idf.py to fetch managed_components, or install the nlohmann-json3-dev system package")
endif()

idf_component_register(
        SRCS "types.cpp" "serializer.cpp" "geo_helper.cpp"
        INCLUDE_DIRS "." "${NLOHMANN_JSON_INCLUDE_DIR}"
)
endif()