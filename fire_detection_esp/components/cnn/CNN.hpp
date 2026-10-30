#pragma once
#include <string>

#include "esp_err.h"
#include <vector>

class CNN {
public:
    CNN();
    ~CNN() = delete;
private:
    const char * TAG = "CNN";

    esp_err_t loadModel();

    std::vector<std::string> labels;


public:
    esp_err_t init();

private:

};
