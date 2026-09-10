FILE(GLOB frontend_bl_srcs "*.cpp")

idf_component_register(
        SRCS ${frontend_bl_srcs}
        INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
        REQUIRES bt nvs_flash esp_driver_uart freertos flight_data barometer
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