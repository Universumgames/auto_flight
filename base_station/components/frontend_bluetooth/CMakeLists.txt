FILE(GLOB frontend_bl_srcs "*.cpp")

idf_component_register(
        SRCS ${frontend_bl_srcs}
        INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
        REQUIRES bt nvs_flash esp_driver_uart freertos barometer nlohmann-json proxy_cache device_id flight_com battery
)

# Regenerates build_timestamp.h (BUILD_EPOCH_TIMESTAMP, see BluetoothManager::startAdvertising)
# on every build, not just every CMake configure — add_custom_target has no file-based OUTPUT,
# so Ninja/Make always considers it out of date and reruns it, which in turn bumps the
# generated header's mtime and triggers a recompile of whatever includes it.
add_custom_target(frontend_bluetooth_build_timestamp
        COMMAND ${CMAKE_COMMAND} "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/build_timestamp.h"
                -P "${CMAKE_CURRENT_LIST_DIR}/generate_build_timestamp.cmake"
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/build_timestamp.h"
        COMMENT "Stamping frontend_bluetooth build timestamp"
)
add_dependencies(${COMPONENT_LIB} frontend_bluetooth_build_timestamp)

# Regenerates BLETopics.generated.hpp from ble_protocol.json on every build (see
# gen_ble_protocol.py) — the single source of truth for the BLE topic/UUID table,
# fragmentation header layout and manufacturer-data layout, otherwise mirrored by hand
# on iOS (BluetoothUUIDs.generated.swift) and, once it exists, Android (BleProtocol.generated.kt).
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_target(frontend_bluetooth_ble_protocol
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/gen_ble_protocol.py"
                --schema "${CMAKE_CURRENT_LIST_DIR}/ble_protocol.json"
                --out-cpp "${CMAKE_CURRENT_BINARY_DIR}/BLETopics.generated.hpp"
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/BLETopics.generated.hpp"
        DEPENDS "${CMAKE_CURRENT_LIST_DIR}/ble_protocol.json" "${CMAKE_CURRENT_LIST_DIR}/gen_ble_protocol.py"
                "${CMAKE_CURRENT_LIST_DIR}/../../shared_components/flight_com/packets/base.hpp"
        COMMENT "Generating BLETopics.generated.hpp from ble_protocol.json"
)
add_dependencies(${COMPONENT_LIB} frontend_bluetooth_ble_protocol)