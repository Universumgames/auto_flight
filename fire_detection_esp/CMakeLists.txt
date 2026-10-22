# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
cmake_minimum_required(VERSION 3.16)

set(ENV{IDF_COMPONENT_SUPPRESS_UNKNOWN_FILE_WARNINGS} 1)
set(ENV{KCONFIG_REPORT_VERBOSITY} quiet)

add_compile_definitions(FLIGHT_DEVICE_TYPE_PLANE=1)
set(FLIGHT_DEVICE_TYPE_PLANE 1)

set(EXTRA_COMPONENT_DIRS components shared_components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fire_detection_esp)
