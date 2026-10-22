#pragma once
#include "ads111x.h"

#include <array>

#define MAX_BATTERY_CELL_COUNT 3

class BatteryClass {
private:
    BatteryClass() = default;

public:
    ~BatteryClass() = delete;

    static BatteryClass& getInstance();
    static BatteryClass* getInstancePtr();

    void begin(char cellCount = MAX_BATTERY_CELL_COUNT);

    /// Cheap check for whether the ADC is present and responding on the bus,
    /// without touching TAG_BATTERY logging. Use this before relying on the
    /// other getters if the sense board may not be connected.
    [[nodiscard]] bool isAvailable() const;

    // The 4S pack is sensed through its balance leads: AIN0 taps B- to cell1+,
    // AIN1 taps to cell2+, AIN2 to cell3+, AIN3 to cell4+/pack+. Each channel
    // therefore reads the *cumulative* voltage from pack negative, so the
    // per-cell voltage is the difference between successive channels.
    /// Voltage of each individual cell in millivolts, index 0 = cell nearest B-.
    std::array<int, MAX_BATTERY_CELL_COUNT> getCellVoltagesMillivolts();

    /// Average per-cell voltage in millivolts, for feeding voltageToPercentage()
    /// which is calibrated against a single LiPo cell's discharge curve.
    uint32_t getVoltageMillivolts();

    uint8_t getVoltagePercentage() {
        lastMeasuredVoltagePercentage = voltageToPercentage(getVoltageMillivolts());
        return lastMeasuredVoltagePercentage;
    }

    [[nodiscard]] uint8_t getLastMeasuredVoltagePercentage() const {
        return lastMeasuredVoltagePercentage;
    }

    /// Convert a raw per-cell voltage reading to an estimated charge percentage (0-100).
    static uint8_t voltageToPercentage(uint32_t millivolts);

private:
    // Per-cell thresholds for the 3S 1600mAh 11.1V (3.7V nominal) LiPo pack.
    // Full is the standard 4.2V charge cutoff; empty is 3.5V rather than the
    // 3.0V absolute minimum, so 0% leaves margin to land before the cells
    // take damage (voltage also sags under load, reading lower than resting).
    static constexpr uint32_t emptyVoltageMillivolts = 3500;
    static constexpr uint32_t fullVoltageMillivolts = 4200;

    // Each AINx-to-GND channel goes through a 47k (top) / 10k (bottom) divider,
    // so V_adc = V_tap * 10k / (47k + 10k); un-scale by (47k + 10k) / 10k.
    static constexpr float voltageDividerRatio = (47.0f + 10.0f) / 10.0f;

    // Conversions can take a few ms; bound the busy-poll so a stuck/disconnected
    // ADC can't hang whoever is reading the battery state.
    static constexpr uint32_t maxConversionPollAttempts = 20;

    static const char* TAG_BATTERY;

    /// Starts a conversion on the given single-ended channel, waits for it to
    /// finish and returns the un-scaled (real, post-divider-correction) voltage
    /// in millivolts.
    uint32_t readChannelMillivolts(ads111x_mux_t mux);

    ads111x_handle_t adsHandle = nullptr;
    uint8_t batteryCellCount = MAX_BATTERY_CELL_COUNT;
    uint8_t lastMeasuredVoltagePercentage = 0;
};

extern BatteryClass& Battery;
