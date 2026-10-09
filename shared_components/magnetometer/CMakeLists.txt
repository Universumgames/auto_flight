
if (NOT NATIVE_BUILD)
    FILE(GLOB magnetometer_srcs "*.cpp")

    idf_component_register(
            SRCS ${magnetometer_srcs}
            INCLUDE_DIRS "."
            REQUIRES driver freertos esp_timer mbedtls esp_driver_gpio pthread i2c_manager hmc5883l qmc5883p qmc5883p_eil
    )
endif ()
