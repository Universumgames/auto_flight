if(NOT NATIVE_BUILD)

FILE(GLOB flight_control_srcs "*.cpp")

idf_component_register(
        SRCS ${flight_control_srcs}
        INCLUDE_DIRS "."
        REQUIRES gps barometer flight_data lora_com gyroscope flight_com route_planner homog2d motor_com_master magnetometer oled_display
)

endif()