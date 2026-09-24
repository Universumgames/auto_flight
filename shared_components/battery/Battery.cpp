#include "Battery.hpp"

#include <algorithm> // std::min, std::max

#include "freertos/FreeRTOS.h"
#include "i2c_manager.hpp"

static BatteryClass* batteryInstance = nullptr;

BatteryClass& Battery = BatteryClass::getInstance();

const char* BatteryClass::TAG_BATTERY = "Battery";

BatteryClass* BatteryClass::getInstancePtr() {
    if (!batteryInstance) {
        batteryInstance = new BatteryClass();
    }
    return batteryInstance;
}

BatteryClass& BatteryClass::getInstance() {
    return *getInstancePtr();
}

void BatteryClass::begin(char cellCount) {
    batteryCellCount = cellCount;
    adsHandle = ads111x_create(I2CManager::getBus(), ADS111X_ADDR_GND);

    esp_err_t err = ads111x_set_mode(adsHandle, ADS111X_MODE_SINGLE_SHOT);
    I2C_ERROR_LOG(TAG_BATTERY, "set mode failed", err);
    err = ads111x_set_data_rate(adsHandle, ADS111X_DATA_RATE_128);
    I2C_ERROR_LOG(TAG_BATTERY, "set data rate failed", err);
    err = ads111x_set_gain(adsHandle, ADS111X_GAIN_6V144);
    I2C_ERROR_LOG(TAG_BATTERY, "set gain failed", err);
}

bool BatteryClass::isAvailable() const {
    if (!adsHandle) return false;
    bool busy = false;
    return ads111x_is_busy(adsHandle, &busy) == ESP_OK;
}

int BatteryClass::readChannelMillivolts(const ads111x_mux_t mux) {
    esp_err_t err = ads111x_set_input_mux(adsHandle, mux);
    I2C_ERROR_LOG(TAG_BATTERY, "set mux failed", err);
    err = ads111x_start_conversion(adsHandle);
    I2C_ERROR_LOG(TAG_BATTERY, "start conversion failed", err);

    bool busy = true;
    for (int attempt = 0; attempt < maxConversionPollAttempts && busy; attempt++) {
        vTaskDelay(pdMS_TO_TICKS(1));
        err = ads111x_is_busy(adsHandle, &busy);
        I2C_ERROR_LOG(TAG_BATTERY, "busy poll failed", err);
        if (err != ESP_OK) {
            break;
        }
    }

    int16_t raw = 0;
    err = ads111x_get_value(adsHandle, &raw);
    I2C_ERROR_LOG(TAG_BATTERY, "get value failed", err);

    const float adcVolts = (static_cast<float>(raw) / ADS111X_MAX_VALUE) * ads111x_gain_values[ADS111X_GAIN_6V144];
    return static_cast<int>(adcVolts * voltageDividerRatio * 1000.0f);
}

std::array<int, MAX_BATTERY_CELL_COUNT> BatteryClass::getCellVoltagesMillivolts() {
    static constexpr ads111x_mux_t channelMux[MAX_BATTERY_CELL_COUNT] = {
        ADS111X_MUX_0_GND, ADS111X_MUX_1_GND, ADS111X_MUX_2_GND, ADS111X_MUX_3_GND
    };

    std::array<int, MAX_BATTERY_CELL_COUNT> cumulativeMillivolts{};
    for (int i = 0; i < batteryCellCount; i++) {
        //cumulativeMillivolts[i] = readChannelMillivolts(channelMux[i]);
    }

    std::array<int, MAX_BATTERY_CELL_COUNT> cellVoltages{};
    int previous = 0;
    for (int i = 0; i < batteryCellCount; i++) {
        cellVoltages[i] = cumulativeMillivolts[i] - previous;
        previous = cumulativeMillivolts[i];
    }
    return cellVoltages;
}

int BatteryClass::getVoltageMillivolts() {
    const auto cells = getCellVoltagesMillivolts();
    int sum = 0;
    for (const int cellMillivolts : cells) {
        sum += cellMillivolts;
    }
    return sum / batteryCellCount;
}

int BatteryClass::voltageToPercentage(const int millivolts) {
    const int clamped = std::max(emptyVoltageMillivolts, std::min(fullVoltageMillivolts, millivolts));
    return (clamped - emptyVoltageMillivolts) * 100 / (fullVoltageMillivolts - emptyVoltageMillivolts);
}
