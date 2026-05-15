idf_component_register(SRCS "route_planner.cpp" "geo_helper.cpp"
                       INCLUDE_DIRS .
                        REQUIRES minmea homog2d flight_data)