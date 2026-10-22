
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
else ()
    # Flight_Communication.cpp drives the LoRa radio and is ESP-only; the
    # generated packet headers and PacketCodec.cpp's decodePacket are plain C++ and
    # needed by native tests.
    idf_component_register(
            SRCS "PacketCodec.cpp"
            INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
            REQUIRES flight_data
    )
endif ()

# Generates FlightTypes.generated.hpp / FlightPackets.generated.hpp from packets.jtd.json
# (see gen_packets.py) — the single source of truth for every packet, also generated for
# KMP (FrontendPackets*.generated.kt, Gradle). Written to this component's binary dir,
# which is a public include dir above. Done at configure time rather than by a build
# target because the types header reaches many components through types.hpp (gps,
# lora_com, route_planner, ...), which must not compile before it exists; the
# CMAKE_CONFIGURE_DEPENDS entries re-run CMake (and so the generator) whenever the
# schema or the script changes.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/gen_packets.py"
                --schema "${CMAKE_CURRENT_LIST_DIR}/packets.jtd.json"
                --out-cpp "${CMAKE_CURRENT_BINARY_DIR}"
        OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/packets.jtd.json" "${CMAKE_CURRENT_LIST_DIR}/gen_packets.py")

if (NOT NATIVE_BUILD)
    # Regenerates BLETopics.generated.hpp from ble_protocol.json on every build (see
    # gen_ble_protocol.py) — the single source of truth for the BLE topic/UUID table,
    # fragmentation header layout and manufacturer-data layout, also generated for iOS
    # (BluetoothUUIDs.generated.swift, Xcode build phase) and KMP (BleProtocol.generated.kt,
    # Gradle). Written to this component's binary dir, which is a public include dir above,
    # so the base station's frontend_bluetooth (the only user; the header needs NimBLE's
    # host/ble_uuid.h) can include it.
    set(BLE_PROTOCOL_GEN_CMD ${Python3_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/gen_ble_protocol.py"
            --schema "${CMAKE_CURRENT_LIST_DIR}/ble_protocol.json"
            --out-cpp "${CMAKE_CURRENT_BINARY_DIR}/BLETopics.generated.hpp")
    # Also generate once at configure time, so the header exists as soon as the project is
    # loaded (IDEs like CLion resolve includes after a CMake reload, before any build).
    execute_process(COMMAND ${BLE_PROTOCOL_GEN_CMD} OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
    add_custom_target(flight_com_ble_protocol
            COMMAND ${BLE_PROTOCOL_GEN_CMD}
            BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/BLETopics.generated.hpp"
            DEPENDS "${CMAKE_CURRENT_LIST_DIR}/ble_protocol.json" "${CMAKE_CURRENT_LIST_DIR}/gen_ble_protocol.py"
                    "${CMAKE_CURRENT_LIST_DIR}/packets.jtd.json"
            COMMENT "Generating BLETopics.generated.hpp from ble_protocol.json"
    )
    add_dependencies(${COMPONENT_LIB} flight_com_ble_protocol)
endif ()
