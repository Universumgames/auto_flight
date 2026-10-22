FILE(GLOB main_srcs "*.cpp")

idf_component_register(
    SRCS ${main_srcs}
    INCLUDE_DIRS "."
    PRIV_REQUIRES camera cnn scanner_controller freertos
)

# TFLM headers require C++17.
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
