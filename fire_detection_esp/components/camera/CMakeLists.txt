
if (NOT NATIVE_BUILD)
    FILE(GLOB camera_srcs "*.cpp")

    idf_component_register(
            SRCS ${camera_srcs}
            INCLUDE_DIRS "."
            REQUIRES freertos esp_timer flight_com
    )
endif ()