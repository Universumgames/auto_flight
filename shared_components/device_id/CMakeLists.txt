
if (NOT NATIVE_BUILD)
    idf_component_register(
            SRCS "DeviceId.cpp"
            INCLUDE_DIRS "."
            REQUIRES esp_hw_support
    )
else ()
    idf_component_register(
            SRCS "DeviceId_native.cpp"
            INCLUDE_DIRS "."
    )
endif ()
