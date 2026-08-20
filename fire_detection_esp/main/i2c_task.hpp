#pragma once

#include <cstdint>

#include "esp_err.h"

// Starts a background FreeRTOS task that owns the I2C bus and polls
// whatever sensor(s) you wire in (see SENSOR_I2C_ADDR / read_register in
// i2c_task.cpp -- this is a template, not a real driver). Call once from
// app_main(), after which it runs independently of the inference task.
esp_err_t i2c_task_start();

// Thread-safe snapshot of the most recent reading. Returns false if no
// reading has completed yet.
bool i2c_task_get_latest(uint8_t *value_out);
