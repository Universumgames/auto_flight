# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{ESP_IDF_VERSION})
    set(EXTRA_COMPONENT_DIRS components shared_components)

    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(flight_controller)
else()
    # Global compile definition for all targets
    add_compile_definitions(NATIVE_BUILD=1)
    set(NATIVE_BUILD 1)
    include(test/CMakeLists.txt)
endif()