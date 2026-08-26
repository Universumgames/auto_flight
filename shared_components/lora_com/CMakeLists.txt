
if (NOT NATIVE_BUILD)
    FILE(GLOB lora_com_srcs "*.cpp")

    idf_component_register(
            SRCS ${lora_com_srcs}
            #SRCS "LoRa_Communication.cpp"
            INCLUDE_DIRS "."
            REQUIRES driver freertos esp_timer mbedtls esp_driver_gpio radiolib pthread
    )
else ()
    # LoRa_Communication.cpp drives the SX126x radio via radiolib/FreeRTOS and
    # is ESP-only; LoRaPacket.hpp is a plain struct needed by native tests.
    idf_component_register(
            INCLUDE_DIRS "."
    )
endif ()