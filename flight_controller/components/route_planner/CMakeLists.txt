idf_component_register(SRCS "route_planner.cpp" "geo_helper.cpp"
                       INCLUDE_DIRS .
                        REQUIRES  flight_data
                        PRIV_REQUIRES homog2d)