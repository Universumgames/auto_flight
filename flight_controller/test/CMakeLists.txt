project(flight_controller_test C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GoogleTest)

find_package(GTest REQUIRED)

enable_testing()

include(../cmake/idf_compat.cmake)

# ---------------------------------------------------------
# nlohmann-json (>= 3.12 needed for NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE)
# Distro packages are often older, so fall back to fetching it
# ---------------------------------------------------------

find_package(nlohmann_json 3.12.0 QUIET)

if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json >= 3.12.0 not found, fetching it")
    include(FetchContent)
    FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(json)
endif()

# CONFIGURE_DEPENDS: new test files get picked up on the next build / CLion reload
file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/test/*.cpp)

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

# test/test.cpp provides main(), so link plain gtest (not GTest::Main)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest)

gtest_discover_tests(${PROJECT_NAME})

# ---------------------------------------------------------
# Share all component includes globally
# ---------------------------------------------------------

foreach(tgt ${IDF_COMPONENT_TARGETS})

    target_include_directories(${tgt} PUBLIC
        ${IDF_ALL_INCLUDE_DIRS}
    )

    # Stand-in for the johboh/nlohmann-json IDF component
    target_link_libraries(${tgt} PUBLIC nlohmann_json::nlohmann_json)

endforeach()