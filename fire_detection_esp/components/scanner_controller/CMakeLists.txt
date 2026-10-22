
if (NOT NATIVE_BUILD)
    FILE(GLOB scanner_controller_srcs "*.cpp")

    idf_component_register(
            SRCS ${scanner_controller_srcs}
            INCLUDE_DIRS "."
            REQUIRES freertos esp_timer flight_com cnn camera
    )
endif ()