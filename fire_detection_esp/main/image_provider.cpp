#include "image_provider.hpp"

namespace {
uint32_t g_frame_counter = 0;
}

// TODO(camera): once you've picked a camera module, replace this body with
// something like:
//
//   #include "esp_camera.h"
//   camera_fb_t *fb = esp_camera_fb_get();
//   if (!fb) return ESP_FAIL;
//   // fb->format depends on camera config (e.g. PIXFORMAT_RGB888, or
//   // PIXFORMAT_JPEG needing fmt2rgb888() from esp32-camera's img_converters).
//   // Also resize/crop fb->buf (likely larger than the model's input) down
//   // to width x height, e.g. with esp32-camera's dl_image or a simple
//   // nearest-neighbor downsample, before copying into out_rgb888.
//   esp_camera_fb_return(fb);
//
// esp32-camera itself is a separate managed component
// (espressif/esp32-camera) and needs its own idf_component.yml entry plus
// pin configuration for your specific board -- not added here since no
// board was chosen yet.
esp_err_t image_provider_get_frame(uint8_t *out_rgb888, size_t out_len, int width, int height) {
    if (out_len != (size_t)(width * height * 3)) {
        return ESP_ERR_INVALID_SIZE;
    }

    // Deterministic, slowly-shifting test pattern so you can see the
    // pipeline actually run (varying input -> varying inference output)
    // without any camera hardware attached.
    g_frame_counter++;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t *px = out_rgb888 + (y * width + x) * 3;
            px[0] = (uint8_t)(x * 255 / width);
            px[1] = (uint8_t)(y * 255 / height);
            px[2] = (uint8_t)((g_frame_counter * 8) & 0xFF);
        }
    }
    return ESP_OK;
}
