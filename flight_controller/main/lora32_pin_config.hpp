#pragma once
#include <driver/gpio.h>

#define PIN_LORA_NSS ((gpio_num_t)8)
#define PIN_LORA_SCK ((gpio_num_t)9)
#define PIN_LORA_MISO ((gpio_num_t)10) // LoRa_MOSI
#define PIN_LORA_MOSI ((gpio_num_t)11) // LoRa_MISO
#define PIN_LORA_RST ((gpio_num_t)12)
#define PIN_LORA_BUSY ((gpio_num_t)13)
#define PIN_LORA_DIO0 ((gpio_num_t)14)