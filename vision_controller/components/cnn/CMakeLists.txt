
if (NOT NATIVE_BUILD)
    FILE(GLOB cnn_srcs "*.cpp")

    idf_component_register(
            SRCS ${cnn_srcs}
            INCLUDE_DIRS "."
            REQUIRES freertos esp_timer flight_com camera
    )
endif ()