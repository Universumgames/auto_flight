#include "inference.hpp"

#include <cmath>

#include "esp_log.h"
#include "model_data.hpp"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {
constexpr char TAG[] = "fire_inference";

// Sized for the default tiny model in fire_detection_training/model.py at
// 96x96x3 input (a few hundred KB, mostly activations/scratch, not
// weights). Grow this if you widen the model or bump IMAGE_SIZE, and watch
// AllocateTensors() below for failures. On a plain ESP32 (no PSRAM) this
// eats most of the ~320KB of internal DRAM; on an S3 with PSRAM you have a
// lot more room (move g_tensor_arena into PSRAM via
// heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM) if needed).
constexpr int kTensorArenaSize = 200 * 1024;
alignas(16) uint8_t g_tensor_arena[kTensorArenaSize];

const tflite::Model *g_model = nullptr;
tflite::MicroInterpreter *g_interpreter = nullptr;
TfLiteTensor *g_input = nullptr;
TfLiteTensor *g_output = nullptr;
bool g_ready = false;
}  // namespace

esp_err_t fire_inference_init() {
    if (g_model_data_len == 0) {
        ESP_LOGE(TAG, "No model embedded -- run export_c_array.py from fire_detection_training first.");
        return ESP_ERR_INVALID_STATE;
    }

    g_model = tflite::GetModel(g_model_data);
    if (g_model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Model schema version %u != supported %u", (unsigned)g_model->version(),
                 (unsigned)TFLITE_SCHEMA_VERSION);
        return ESP_ERR_INVALID_VERSION;
    }

    // Ops registered here must match exactly what fire_detection_training/model.py
    // compiles down to -- this list was read off the exported .tflite via
    // interpreter._get_ops_details() (ADD/MUL come from folded BatchNorm
    // scale+shift that didn't fully fuse into the preceding conv's bias).
    // If you change model.py's architecture, re-check that list and update
    // both the op registrations and the resolver's template count below.
    static tflite::MicroMutableOpResolver<7> resolver;
    resolver.AddAdd();
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddFullyConnected();
    resolver.AddMean();
    resolver.AddMul();
    resolver.AddSoftmax();

    static tflite::MicroInterpreter interpreter(g_model, resolver, g_tensor_arena, kTensorArenaSize);
    g_interpreter = &interpreter;

    if (g_interpreter->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors() failed -- tensor arena (%d bytes) is likely too small.", kTensorArenaSize);
        return ESP_ERR_NO_MEM;
    }

    g_input = g_interpreter->input(0);
    g_output = g_interpreter->output(0);
    g_ready = true;

    ESP_LOGI(TAG, "Model ready. Input: %dx%dx%d, arena used: %u/%d bytes, labels: %u",
             (int)g_input->dims->data[1], (int)g_input->dims->data[2], (int)g_input->dims->data[3],
             (unsigned)g_interpreter->arena_used_bytes(), kTensorArenaSize, g_model_labels_len);
    return ESP_OK;
}

void fire_inference_get_input_size(int *width_out, int *height_out) {
    if (!g_ready) {
        *width_out = 0;
        *height_out = 0;
        return;
    }
    *height_out = g_input->dims->data[1];
    *width_out = g_input->dims->data[2];
}

esp_err_t fire_inference_run(const uint8_t *rgb888_image, size_t image_len, fire_inference_result_t *result_out) {
    if (!g_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (image_len != g_input->bytes) {
        ESP_LOGE(TAG, "image_len %u != model input size %u", (unsigned)image_len, (unsigned)g_input->bytes);
        return ESP_ERR_INVALID_ARG;
    }

    // Quantize raw uint8 [0,255] RGB888 into the model's actual int8 input
    // range (scale/zero_point come from the training-time representative
    // dataset in convert_to_tflite.py -- must NOT be hardcoded, since the
    // exact quantization the converter picked isn't guaranteed to be a
    // clean +/-128 shift). This mirrors evaluate.py's quantize_input().
    const float in_scale = g_input->params.scale;
    const int in_zero_point = g_input->params.zero_point;
    int8_t *dst = g_input->data.int8;
    for (size_t i = 0; i < image_len; ++i) {
        int32_t quantized = lroundf((float)rgb888_image[i] / in_scale) + in_zero_point;
        if (quantized < -128) quantized = -128;
        if (quantized > 127) quantized = 127;
        dst[i] = (int8_t)quantized;
    }

    if (g_interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Invoke() failed");
        return ESP_FAIL;
    }

    const float out_scale = g_output->params.scale;
    const int out_zero_point = g_output->params.zero_point;

    int best_index = 0;
    float best_score = -1.0f;
    const int num_classes = g_output->dims->data[1];
    for (int i = 0; i < num_classes; ++i) {
        float score = (g_output->data.int8[i] - out_zero_point) * out_scale;
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }

    result_out->label = (best_index < (int)g_model_labels_len) ? g_model_labels[best_index] : "?";
    result_out->confidence = best_score;
    return ESP_OK;
}
