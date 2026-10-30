#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

struct fire_inference_result_t {
    const char *label;
    float confidence;  // model's softmax score for `label`, roughly 0..1
};

// Loads the embedded model and allocates the TFLM interpreter + tensor
// arena. Call once at startup. Returns ESP_ERR_INVALID_STATE if no model
// has been embedded yet -- see model_data.cpp and
// ../../fire_detection_training/README.md.
esp_err_t fire_inference_init();

// Width/height the model expects (0,0 if fire_inference_init() hasn't
// succeeded yet). Use this to size the buffer passed to fire_inference_run.
void fire_inference_get_input_size(int *width_out, int *height_out);

// Runs one inference on an RGB888 image of exactly
// width * height * 3 bytes (see fire_inference_get_input_size).
esp_err_t fire_inference_run(const uint8_t *rgb888_image, size_t image_len, fire_inference_result_t *result_out);
