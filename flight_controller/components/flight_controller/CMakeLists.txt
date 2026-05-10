FILE(GLOB flight_control_srcs "*.cpp")

idf_component_register(
        SRCS ${flight_control_srcs}
        INCLUDE_DIRS "."
)