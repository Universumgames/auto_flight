#pragma once
#include <freertos/FreeRTOS.h>
#include <hal/uart_types.h>
#include <soc/gpio_num.h>
#include <string>
#include <driver/uart.h>
#include <freertos/queue.h>

#include "minmea.h"


class GPS_Reader {
public:
    GPS_Reader(gpio_num_t rx_pin = (gpio_num_t) 39,  gpio_num_t tx_pin = (gpio_num_t) 6, uart_port_t uart_num = UART_NUM_2);

    void begin();
    ~GPS_Reader();

    [[nodiscard]] bool available() const;

    void getCurrentCoordinates(float& x, float& y, float& z);

    float getCurrentSpeed();

    [[nodiscard]] tm getCurrentTime() const;

private:
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
            .allow_pd = true,
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

public:
    /// internal callback queue for UART events
    QueueHandle_t uart_queue;
    uart_port_t uartNum;


    void handleReceive(const std::string& line);

};
