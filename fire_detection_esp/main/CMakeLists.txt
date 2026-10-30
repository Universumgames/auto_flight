FILE(GLOB main_srcs "*.cpp")

idf_component_register(
    SRCS ${main_srcs}
    INCLUDE_DIRS "."
    PRIV_REQUIRES camera cnn scanner_controller freertos
)
