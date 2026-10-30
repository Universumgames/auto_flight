#include "CNN.hpp"

#include <model_data.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "tensorflow/lite/micro/micro_interpreter.h"

CNN::CNN() {}

esp_err_t CNN::init() {
    esp_err_t err = loadModel();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load model");
        return err;
    }

    return ESP_OK;
}

esp_err_t CNN::loadModel() {
    labels = std::vector<std::string>(kLabels, kLabels + kNumLabels);

    model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Schema version %lu != supported %d",
                 (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
        return ESP_ERR_INVALID_VERSION;
    }

    if (registerModelOps(micro_mutable_op_resolver) != kTfLiteOk) {
        ESP_LOGE(TAG, "Failed to register the model's ops");
        return ESP_ERR_INVALID_STATE;
    }

    tensor_arena = (uint8_t*)heap_caps_aligned_alloc(16, kArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (tensor_arena == nullptr) {
        ESP_LOGW(TAG, "Failed to allocate tensor arena of size %d in RAM", kArenaSize);
        tensor_arena = (uint8_t*)heap_caps_aligned_alloc(16, kArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (tensor_arena == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate tensor arena of size %d in SPIRAM", kArenaSize);
            return ESP_ERR_NO_MEM;
        }
    }

    interpreter = new tflite::MicroInterpreter(model, micro_mutable_op_resolver, tensor_arena, kArenaSize);
    if (interpreter == nullptr) {
        ESP_LOGE(TAG, "Failed to create interpreter");
        return ESP_ERR_NO_MEM;
    }

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        ESP_LOGE(TAG, "Failed to allocate tensors");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Arena used: %u / %u bytes", (unsigned) interpreter->arena_used_bytes(), (unsigned) kArenaSize);

    input = interpreter->input(0);
    output = interpreter->output(0);

    // 6) Print what the model expects. Compare with Python's get_input_details()
    ESP_LOGI(TAG, "Input : type=%d dims=[%d,%d,%d,%d] scale=%f zp=%ld",
             input->type, input->dims->data[0], input->dims->data[1],
             input->dims->data[2], input->dims->data[3],
             input->params.scale, (long)input->params.zero_point);
    ESP_LOGI(TAG, "Output: type=%d elements=%d scale=%f zp=%ld",
             output->type, output->dims->data[output->dims->size - 1],
             output->params.scale, (long)output->params.zero_point);


    const int n_out = output->dims->data[output->dims->size - 1];
    if (n_out != kNumLabels && !(n_out == 1 && kNumLabels == 2)) {
        ESP_LOGE(TAG, "Model has %d outputs but %d labels", n_out, kNumLabels);
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

int8_t CNN::quantize_input(float x) {
    // q = x / scale + zero_point, clamped to int8
    int32_t q = (int32_t)lroundf(x / input->params.scale) + input->params.zero_point;
    if (q < -128) q = -128;
    if (q >  127) q =  127;
    return (int8_t)q;
}

float CNN::output_value(int i) {
    if (output->type == kTfLiteInt8)   // dequantize: (q - zero_point) * scale
        return (output->data.int8[i] - output->params.zero_point) * output->params.scale;
    if (output->type == kTfLiteFloat32)
        return output->data.f[i];
    return 0.f;
}

bool CNN::inference_run(Prediction* top, float* all_scores)  {
    const int64_t t0 = esp_timer_get_time();
    if (interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Invoke() failed");
        return false;
    }
    const int64_t dt = esp_timer_get_time() - t0;

    const int n_out = output->dims->data[output->dims->size - 1];

    if (n_out == 1) {
        // Binary model with a single sigmoid output: p = P(class 1)
        float p = output_value(0);
        if (all_scores) { all_scores[0] = 1.f - p; all_scores[1] = p; }
        top->index = p >= 0.5f ? 1 : 0;
        top->confidence = p >= 0.5f ? p : 1.f - p;
    } else {
        // Softmax: pick the max (argmax)
        int best = 0; float best_v = output_value(0);
        for (int i = 0; i < n_out; ++i) {
            float v = output_value(i);
            if (all_scores) all_scores[i] = v;
            if (v > best_v) { best_v = v; best = i; }
        }
        top->index = best;
        top->confidence = best_v;
    }
    top->label = kLabels[top->index];
    ESP_LOGD(TAG, "Invoke took %lld ms", dt / 1000);
    return true;
}