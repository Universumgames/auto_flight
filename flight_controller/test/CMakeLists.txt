project(flight_controller_test C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GoogleTest)

find_package(GTest REQUIRED)

include(../cmake/idf_compat.cmake)

FILE(GLOB_RECURSE test_sources ${CMAKE_SOURCE_DIR}/test/*.*)

file(GLOB COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/components/*
    ${CMAKE_CURRENT_SOURCE_DIR}/shared_components/*
)

message(STATUS "Found component directories: ${COMPONENT_DIRS}")

function(add_idf_component dir)

    get_filename_component(name "${dir}" NAME)
    set(binary_dir "${CMAKE_CURRENT_BINARY_DIR}/components/${name}")

    add_subdirectory("${dir}" "${binary_dir}")

    # DO NOT create targets here
    # DO NOT assume register was called

endfunction()

foreach(component_dir ${COMPONENT_DIRS})

    if(IS_DIRECTORY "${component_dir}"
       AND EXISTS "${component_dir}/CMakeLists.txt")

        add_idf_component("${component_dir}")

    endif()

endforeach()

add_executable(${PROJECT_NAME} ${test_sources})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    idf_components
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_link_libraries(${PROJECT_NAME} PRIVATE GTest::GTest GTest::Main)

gtest_add_tests(TARGET ${PROJECT_NAME})

# ---------------------------------------------------------
# Share all component includes globally
# ---------------------------------------------------------

foreach(tgt ${IDF_COMPONENT_TARGETS})

    target_include_directories(${tgt} PUBLIC
        ${IDF_ALL_INCLUDE_DIRS}
    )

endforeach()