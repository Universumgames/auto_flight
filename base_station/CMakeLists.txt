# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(ENV{IDF_COMPONENT_SUPPRESS_UNKNOWN_FILE_WARNINGS} 1)
set(ENV{KCONFIG_REPORT_VERBOSITY} quiet)

set(EXTRA_COMPONENT_DIRS components shared_components)

add_compile_definitions(FLIGHT_DEVICE_TYPE_BASE_STATION)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(base_station)

#littlefs_create_partition_image(storage static FLASH_IN_PROJECT)