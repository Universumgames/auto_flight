#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2c_task.hpp"
#include "image_provider.hpp"
#include "inference.hpp"

namespace {
const char *TAG = "main";

// Runs forever: grab a frame, run the CNN, log the result. Lives in its
// own task (rather than app_main's default task) so it doesn't block, and
// so it can be given a large-ish stack and pinned to a core independently
// of i2c_task (see i2c_task.cpp) -- the two run concurrently and don't
// share any state, so no locking is needed between them.
void inference_task_main(void *arg) {
    (void)arg;

    int width, height;
    fire_inference_get_input_size(&width, &height);
    const size_t image_len = (size_t)(width * height * 3);
    uint8_t *image_buf = static_cast<uint8_t *>(heap_caps_malloc(image_len, MALLOC_CAP_8BIT));
    if (!image_buf) {
        ESP_LOGE(TAG, "Failed to allocate %u byte image buffer", (unsigned)image_len);
        vTaskDelete(nullptr);
        return;
    }

    while (true) {
        if (image_provider_get_frame(image_buf, image_len, width, height) != ESP_OK) {
            ESP_LOGW(TAG, "image_provider_get_frame failed");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        fire_inference_result_t result;
        if (fire_inference_run(image_buf, image_len, &result) == ESP_OK) {
            ESP_LOGI(TAG, "Prediction: %s (score %.2f)", result.label, result.confidence);
        } else {
            ESP_LOGW(TAG, "fire_inference_run failed");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
}  // namespace

extern "C" void app_main() {
    ESP_LOGI(TAG, "fire_detection_esp starting");

    esp_err_t err = fire_inference_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fire_inference_init failed (%s) -- no model embedded yet? "
                       "See fire_detection_training/README.md.", esp_err_to_name(err));
        return;
    }

    err = i2c_task_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_task_start failed: %s", esp_err_to_name(err));
        // Not fatal for the inference path: keep going without I2C.
    }

    // Pinned to core 1, away from i2c_task (core 0) and the WiFi/BT stack
    // (also core 0 by default), since inference is the most CPU-heavy work
    // in this app. Stack is large because TFLM's interpreter and this
    // app's local buffers add up; 8192 is a safe starting point -- if it
    // crashes with a stack-overflow watchdog, bump it further.
    xTaskCreatePinnedToCore(inference_task_main, "inference_task", 8192, nullptr,
                             /*priority=*/5, nullptr, /*core=*/1);
}
