idf_component_register(
        SRCS "GPS_Reader.cpp"
        INCLUDE_DIRS "."
        PRIV_REQUIRES freertos esp_timer esp_driver_gpio
        REQUIRES esp_driver_spi minmea esp_driver_uart
)