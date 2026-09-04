
if (NOT NATIVE_BUILD)
    FILE(GLOB flight_com_srcs "*.cpp")

    idf_component_register(
            SRCS ${flight_com_srcs}
            INCLUDE_DIRS "."
            REQUIRES  driver freertos esp_timer flight_data lora_com battery
            PRIV_REQUIRES nlohmann-json gps barometer
    )

    if (FLIGHT_DEVICE_TYPE_PLANE EQUAL 1)
        idf_component_optional_requires(PRIVATE motor_com_master magnetometer)
    endif ()
else ()
    # Flight_Communication.cpp drives the LoRa radio and is ESP-only; the
    # packets/ headers and PacketCodec.cpp's decodePacket are plain C++ and
    # needed by native tests.
    idf_component_register(
            SRCS "PacketCodec.cpp"
            INCLUDE_DIRS "." "packets"
            REQUIRES flight_data
    )
endif ()