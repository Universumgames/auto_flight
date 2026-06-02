#pragma once
#include <freertos/FreeRTOS.h>
#include <hal/uart_types.h>
#include <soc/gpio_num.h>
#include <string>
#include <vector>
#include <driver/uart.h>
#include <freertos/queue.h>

#include "minmea.h"
#include "types.hpp"

using PositionUpdateCallbackFn = std::function<void(Coordinate, time_t)>;

class GPS_ReaderClass {
private:
    GPS_ReaderClass();
public:
    ~GPS_ReaderClass() = delete;

    static GPS_ReaderClass* getInstancePtr();
    static GPS_ReaderClass &getInstance();
public:

    void begin();

    void getCurrentCoordinates(float& x, float& y, float& z);

    Coordinate getCurrentPosition();

    float getCurrentSpeed();

    [[nodiscard]] tm getLatestTimeStruct() const;

    [[nodiscard]] time_t getGPSLatestTime() const;

    bool hasValidPosition() const;

    void addPositionUpdateCallback(const PositionUpdateCallbackFn& callback_fn);

private:
    bool initialized = false;
    gpio_num_t rxPin, txPin;

    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {
            .allow_pd = false,
            .backup_before_sleep = false
        }
    };

    minmea_sentence_rmc lastRMC;
    minmea_sentence_gga lastGGA;
    minmea_sentence_gst lastGST;
    minmea_sentence_gsv lastGSV;
    minmea_sentence_vtg lastVTG;
    minmea_sentence_zda lastZDA;

    time_t lastUpdateTime;

    std::vector<PositionUpdateCallbackFn> positionUpdateCallbacks;

private:
    void callPositionUpdateCallbacks();

public:
    /// internal callback queue for UART events
    QueueHandle_t uart_queue;
    uart_port_t uartNum;


    void handleReceive(const std::string& line);
};

extern GPS_ReaderClass& GPS_Reader;