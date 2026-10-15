
if (NOT NATIVE_BUILD)
    FILE(GLOB flight_com_srcs "*.cpp")

    idf_component_register(
            SRCS ${flight_com_srcs}
            INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
            REQUIRES  driver freertos esp_timer lora_com battery nlohmann-json
            PRIV_REQUIRES gps barometer device_id
    )

    if (FLIGHT_DEVICE_TYPE_PLANE EQUAL 1)
        idf_component_optional_requires(PRIVATE motor_com_master magnetometer)
    endif ()

    # Regenerates BLETopics.generated.hpp from ble_protocol.json on every build (see
    # gen_ble_protocol.py) — the single source of truth for the BLE topic/UUID table,
    # fragmentation header layout and manufacturer-data layout, also generated for iOS
    # (BluetoothUUIDs.generated.swift, Xcode build phase) and KMP (BleProtocol.generated.kt,
    # Gradle). Written to this component's binary dir, which is a public include dir above,
    # so the base station's frontend_bluetooth (the only user; the header needs NimBLE's
    # host/ble_uuid.h) can include it.
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_custom_target(flight_com_ble_protocol
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/gen_ble_protocol.py"
                    --schema "${CMAKE_CURRENT_LIST_DIR}/ble_protocol.json"
                    --out-cpp "${CMAKE_CURRENT_BINARY_DIR}/BLETopics.generated.hpp"
            BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/BLETopics.generated.hpp"
            DEPENDS "${CMAKE_CURRENT_LIST_DIR}/ble_protocol.json" "${CMAKE_CURRENT_LIST_DIR}/gen_ble_protocol.py"
                    "${CMAKE_CURRENT_LIST_DIR}/packets/base.hpp"
            COMMENT "Generating BLETopics.generated.hpp from ble_protocol.json"
    )
    add_dependencies(${COMPONENT_LIB} flight_com_ble_protocol)
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