idf_component_register(
        SRCS "CameraCommunication.cpp"
        INCLUDE_DIRS "."
        REQUIRES esp_driver_i2c i2c_bus freertos flight_com
)
