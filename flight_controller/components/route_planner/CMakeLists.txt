FILE(GLOB route_planner_srcs "*.cpp")

idf_component_register(
        SRCS ${route_planner_srcs}
        INCLUDE_DIRS .
        REQUIRES flight_data
        PRIV_REQUIRES homog2d)