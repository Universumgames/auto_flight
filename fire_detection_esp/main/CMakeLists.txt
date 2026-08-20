idf_component_register(
    SRCS "main.cpp" "inference.cpp" "image_provider.cpp" "model_data.cpp" "i2c_task.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES "esp-tflite-micro" "driver" "esp_common"
)

# TFLM headers require C++17.
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
