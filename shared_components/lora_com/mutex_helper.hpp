#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WITH_MUTEX_CUSTOM_DELAY(mutex, delay)        \
for (bool _once = (xSemaphoreTake((mutex), delay) == pdTRUE); \
_once; \
_once = false, xSemaphoreGive((mutex)))

#define WITH_MUTEX(mutex) \
WITH_MUTEX_CUSTOM_DELAY(mutex, portMAX_DELAY)

#define WITH_MUTEX_ELSE(mutex, code, elseCode)        \
do {                               \
if (xSemaphoreTake((mutex), portMAX_DELAY) == pdTRUE) { \
code                       \
xSemaphoreGive((mutex));   \
} else { \
elseCode                   \
}                              \
} while (0)


#define WITH_MUTEX_ISR(mutex)        \
    for(bool _once = (xSemaphoreTakeFromISR((mutex), nullptr) == pdTRUE); \
    _once; \
    _once = false, xSemaphoreGiveFromISR((mutex), nullptr))