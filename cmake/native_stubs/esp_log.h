#pragma once
// Minimal stand-in for ESP-IDF's esp_log.h when building natively (NATIVE_BUILD).
// Routes ESP_LOGx calls to stdio so component sources don't need an #ifdef
// around every log call just to be testable off-target.

#include <cstdio>

#define ESP_LOGE(tag, fmt, ...) std::printf("[E] %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) std::printf("[W] %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) std::printf("[I] %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) std::printf("[D] %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) std::printf("[V] %s: " fmt "\n", tag, ##__VA_ARGS__)
