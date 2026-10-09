#pragma once
#include <functional>
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

    /**
     * @brief Initializes the GPS reader, sets up UART communication and starts the reading task.
     */
    void begin();

    /**
     * @brief Retrieves the current coordinates (latitude, longitude, altitude) from the GPS reader.
     * @param x Reference to a float variable to store the latitude.
     * @param y Reference to a float variable to store the longitude.
     * @param z Reference to a float variable to store the altitude.
     */
    void getCurrentCoordinates(float& x, float& y, float& z);

    /**
     * @brief Retrieves the current position as a Coordinate struct. If the GPS fix is invalid, the returned coordinates will be (-400, -400).
     * @return A Coordinate struct containing the current latitude and longitude.
     */
    Coordinate getCurrentPosition();

    float getCurrentSpeed();

    /**
     * @brief Retrieves the latest time from the GPS reader as a tm struct.
     * @return A tm struct representing the latest time.
     */
    [[nodiscard]] tm getLatestTimeStruct() const;

    /**
     * @brief Retrieves the latest time from the GPS reader as a time_t value.
     * @return A time_t value representing the latest time.
     */
    [[nodiscard]] time_t getGPSLatestTime() const;

    /**
     * @brief Checks if the GPS reader has a valid position fix.
     * @return true if the GPS reader has a valid position fix, false otherwise.
     */
    bool hasValidPosition() const;

    /**
     * @brief Registers a callback function to be called whenever the GPS position is updated.
     * @param callback_fn The callback function to register. It should take a Coordinate and a time_t as parameters.
     */
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

    time_t lastUpdateTime = 0;

    std::vector<PositionUpdateCallbackFn> positionUpdateCallbacks;

private:
    void callPositionUpdateCallbacks();

    [[noreturn]] static void gps_readerTask(void* param);

    /// internal callback queue for UART events
    QueueHandle_t uart_queue;
    uart_port_t uartNum;


    void handleReceive(const std::string& line);
};

extern GPS_ReaderClass& GPS_Reader;