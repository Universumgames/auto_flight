# -----------------------------------------------------------------------------
# ESP-IDF compatibility layer
# -----------------------------------------------------------------------------

include(CMakeParseArguments)

# Global interface target collecting all component properties
add_library(idf_components INTERFACE)

# -----------------------------------------------------------------------------
# Mock idf_component_register()
# -----------------------------------------------------------------------------

function(idf_component_register)

    set(options)

    set(oneValueArgs)

    set(multiValueArgs
        SRCS
        INCLUDE_DIRS
        PRIV_INCLUDE_DIRS
        REQUIRES
        PRIV_REQUIRES
        COMPILE_DEFINITIONS
        COMPILE_OPTIONS
    )

    cmake_parse_arguments(IDF
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    # ---------------------------------------------------------
    # Create unique library name from directory
    # ---------------------------------------------------------

    get_filename_component(
        COMPONENT_NAME
        ${CMAKE_CURRENT_SOURCE_DIR}
        NAME
    )

    set(TARGET_NAME "idf_component_${COMPONENT_NAME}")

    # ---------------------------------------------------------
    # Resolve source files
    # ---------------------------------------------------------

    set(ABS_SRCS "")

    foreach(src ${IDF_SRCS})

        get_filename_component(
            ABS_SRC
            "${src}"
            ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        list(APPEND ABS_SRCS "${ABS_SRC}")

    endforeach()

    # ---------------------------------------------------------
    # 🔥 Header-only fallback: generate empty .cpp
    # ---------------------------------------------------------

    if(ABS_SRCS STREQUAL "")
        set(DUMMY_CPP "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_dummy.cpp")

        if(NOT EXISTS "${DUMMY_CPP}")
            file(WRITE "${DUMMY_CPP}" "// Auto-generated dummy source for header-only component: ${COMPONENT_NAME}\n"
            )
        endif()

        list(APPEND ABS_SRCS "${DUMMY_CPP}")
    endif()

    # ---------------------------------------------------------
    # Create component library
    # ---------------------------------------------------------

    add_library(${TARGET_NAME} STATIC
        ${ABS_SRCS}
    )

    # ---------------------------------------------------------
    # Includes
    # ---------------------------------------------------------

    foreach(inc ${IDF_INCLUDE_DIRS})

        get_filename_component(
            ABS_INC
            "${inc}"
            ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        target_include_directories(
            ${TARGET_NAME}
            PUBLIC
            "${ABS_INC}"
        )

    endforeach()

    foreach(inc ${IDF_PRIV_INCLUDE_DIRS})

        get_filename_component(
            ABS_INC
            "${inc}"
            ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        target_include_directories(
            ${TARGET_NAME}
            PRIVATE
            "${ABS_INC}"
        )

    endforeach()

    # ---------------------------------------------------------
    # Register includes globally
    # ---------------------------------------------------------

    foreach(inc ${IDF_INCLUDE_DIRS})
        get_filename_component(
            ABS_INC "${inc}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        list(APPEND IDF_ALL_INCLUDE_DIRS "${ABS_INC}")
    endforeach()

    set(IDF_ALL_INCLUDE_DIRS
        "${IDF_ALL_INCLUDE_DIRS}"
        CACHE INTERNAL "All component include dirs"
    )

    # ---------------------------------------------------------
    # Compile definitions/options
    # ---------------------------------------------------------

    target_compile_definitions(
        ${TARGET_NAME}
        PUBLIC
        ${IDF_COMPILE_DEFINITIONS}
    )

    target_compile_options(
        ${TARGET_NAME}
        PUBLIC
        ${IDF_COMPILE_OPTIONS}
    )

    # ---------------------------------------------------------
    # Native build define
    # ---------------------------------------------------------

    target_compile_definitions(
        ${TARGET_NAME}
        PUBLIC
        NATIVE_BUILD=1
    )

    # ---------------------------------------------------------
    # Link dependencies
    # ---------------------------------------------------------

    foreach(dep ${IDF_REQUIRES} ${IDF_PRIV_REQUIRES})

        if(TARGET "idf_component_${dep}")

            target_link_libraries(
                ${TARGET_NAME}
                PUBLIC
                "idf_component_${dep}"
            )

        endif()

    endforeach()

    # ---------------------------------------------------------
    # Register into global collector target
    # ---------------------------------------------------------

    target_link_libraries(
        idf_components
        INTERFACE
        ${TARGET_NAME}
    )

    list(APPEND IDF_COMPONENT_TARGETS ${TARGET_NAME})
    set(IDF_COMPONENT_TARGETS
        "${IDF_COMPONENT_TARGETS}"
        CACHE INTERNAL "All component targets"
    )

endfunction()