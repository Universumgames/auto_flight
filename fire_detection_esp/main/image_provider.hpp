#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

// Fills out_rgb888 (width * height * 3 bytes) with one frame.
//
// No camera has been wired in yet (see README.md): this currently fills
// the buffer with a deterministic synthetic test pattern, which is enough
// to exercise fire_inference_run() end-to-end on real hardware before you
// commit to a camera module. Swap the body of image_provider_get_frame()
// in image_provider.cpp for an esp32-camera capture once you have one.
esp_err_t image_provider_get_frame(uint8_t *out_rgb888, size_t out_len, int width, int height);
