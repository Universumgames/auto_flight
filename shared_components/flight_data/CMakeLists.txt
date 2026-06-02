
if(NOT NATIVE_BUILD)
FILE(GLOB flight_data_srcs "*.cpp")

idf_component_register(
        SRCS ${flight_data_srcs}
        INCLUDE_DIRS "."
        REQUIRES nlohmann-json freertos esp_timer gps
)
endif()