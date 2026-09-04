
if(NOT NATIVE_BUILD)
FILE(GLOB battery_srcs "*.cpp")

idf_component_register(
        SRCS ${battery_srcs}
        INCLUDE_DIRS "."
        REQUIRES esp_timer freertos i2c_manager ads111x
)
endif()
