
if(NOT NATIVE_BUILD)
idf_component_register(
        SRCS "LoRa_Communication.cpp"
        INCLUDE_DIRS "."
        REQUIRES driver freertos esp_timer
)
endif()