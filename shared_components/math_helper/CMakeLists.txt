FILE(GLOB math_srcs "*.cpp")

    idf_component_register(
            SRCS ${math_srcs}
            INCLUDE_DIRS "."
    )