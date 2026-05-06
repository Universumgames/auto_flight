idf_component_register(
        SRCS "serializer.cpp"
        INCLUDE_DIRS "."
        REQUIRES driver freertos esp_timer
)