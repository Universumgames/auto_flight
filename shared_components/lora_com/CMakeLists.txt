idf_register_component(
        SRCS "LoRa_Communication.cpp"
        INCLUDE_DIRS "."
        REQUIRES driver freertos esp_timer
)