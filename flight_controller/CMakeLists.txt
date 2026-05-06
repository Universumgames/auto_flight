cmake_minimum_required(VERSION 3.16.0)

set(EXTRA_COMPONENT_DIRS $(PROJECT_PATH)/../shared_components/)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(flight_controller)
