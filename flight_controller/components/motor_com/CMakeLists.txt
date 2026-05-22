idf_component_register(
        SRCS "MotorCommunication.cpp"
        INCLUDE_DIRS "."
        REQUIRES i2c_manager freertos
)