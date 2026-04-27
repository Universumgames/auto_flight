idf_component_register(
        SRCS "GPS_Reader.cpp"
        INCLUDE_DIRS "."
        REQUIRES minmea driver freertos esp_timer
)