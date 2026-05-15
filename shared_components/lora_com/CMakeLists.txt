
if(NOT NATIVE_BUILD)
    FILE(GLOB lora_com_srcs "*.cpp")

idf_component_register(
        SRCS ${lora_com_srcs}
        #SRCS "LoRa_Communication.cpp"
        INCLUDE_DIRS "."
        REQUIRES driver freertos esp_timer mbedtls esp32-lora-library esp_driver_gpio
)
endif()