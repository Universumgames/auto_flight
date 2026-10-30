#pragma once
#include <string>

#include "esp_err.h"
#include <vector>

#include "esp_log.h"
#include "esp_timer.h"
#include "inference.h"
#include "model_data.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

class CNN {
public:
    CNN();
    ~CNN() = delete;
private:
    const char * TAG = "CNN";
    static constexpr size_t kArenaSize = 300 * 1024;

    const tflite::Model* model = nullptr;
    ModelOpResolver micro_mutable_op_resolver;
    tflite::MicroInterpreter* interpreter;

    TfLiteTensor* input = nullptr;
    TfLiteTensor* output = nullptr;

    uint8_t* tensor_arena = nullptr;

    esp_err_t loadModel();


    std::vector<std::string> labels;


public:
    esp_err_t init();

private:

    int8_t quantize_input(float x);

    float output_value(int i);

    bool inference_run(Prediction* top, float* all_scores);
};
