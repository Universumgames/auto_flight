if(NOT NATIVE_BUILD)
idf_component_register(
    SRCS hmc5883l.c
    INCLUDE_DIRS .
    REQUIRES i2c_bus log esp_timer
)
endif()
