if(NOT NATIVE_BUILD)
idf_component_register(
    SRCS qmc5883p_eil.c
    INCLUDE_DIRS .
    REQUIRES i2c_bus log esp_timer freertos
)
endif()
