# I2C Manager

A small singleton wrapper around ESP-IDF's I2C bus that provides a single, shared `i2c_bus_handle_t` to all components on the flight controller / base station that need to talk to I2C devices (e.g. magnetometer, IMU).

## What it does

- Lazily initializes a single I2C master bus (`I2C_NUM_0`) on first use via `I2CManager::getBus()`.
- Configures SDA/SCL pins and pull-ups, and sets the bus clock to 400 kHz.
- Waits for devices to finish power-on reset before creating the bus, then resets the underlying master bus handle.
- Scans the bus for connected devices and logs how many were found.
- Hands back the same bus handle on every subsequent call, so callers don't need to manage initialization order or ownership themselves.

## Usage

```cpp
#include "i2c_manager.hpp"

i2c_bus_handle_t bus = I2CManager::getBus();
```

## Configuration

Pin assignments are configurable via `idf.py menuconfig` under **I2C Manager Settings**:

- `CONFIG_I2C_PIN_SDA` – GPIO pin for I2C SDA (default: 47)
- `CONFIG_I2C_PIN_SCL` – GPIO pin for I2C SCL (default: 48)

## Dependencies

- ESP-IDF (`>=4.1.0`)
- `esp_driver_i2c` (ESP-IDF component)
- `esp_driver_gpio` (ESP-IDF component, private)
- `freertos` (ESP-IDF component, private)
- [`espressif/i2c_bus`](https://components.espressif.com/components/espressif/i2c_bus) `^1.5.1`
