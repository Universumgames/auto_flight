#pragma once
#include "ads111x.h"

#include <array>

#define MAX_BATTERY_CELL_COUNT 4

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
    bool isAvailable() const;

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
    // TODO: calibrate for the actual battery chemistry/cell count once known.
    static constexpr uint32_t emptyVoltageMillivolts = 3600;
    static constexpr uint32_t fullVoltageMillivolts = 4400;

    // TODO: measure the real sense-board resistor-divider ratio; this assumes
    // every AINx-to-GND channel is scaled down by the same ratio before reaching
    // the ADS111x, and un-scales the reading back up to the real tap voltage.
    static constexpr float voltageDividerRatio = 3.3f;

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
