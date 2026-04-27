//
// Created by Tom Arlt on 21.04.26.
//

#include "GPS_Reader.hpp"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/task.h>
#include <hal/uart_types.h>
#include "minmea.h"

#define BUF_SIZE       1024
#define LINE_BUF_SIZE  256

// Setup UART buffered IO with event queue
constexpr int uart_buffer_size = (1024);

const char* TAG_GPS_READER = "gps_reader";

[[noreturn]] static void gps_readerTask(void* parameters) {
    auto* gps_reader = (GPS_Reader*)parameters;

    uart_event_t event;
    uint8_t data[128];

    static char line_buf[LINE_BUF_SIZE];
    int line_pos = 0;

    auto uart_queue = gps_reader->uart_queue;

    while (true) {
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY)) {

            switch (event.type) {

            case UART_DATA: {
                int len = uart_read_bytes(gps_reader->uartNum, data, event.size, portMAX_DELAY);

                for (int i = 0; i < len; i++) {
                    char c = (char)data[i];

                    // Line ending detected
                    if (c == '\n') {
                        line_buf[line_pos] = '\0';
                        gps_reader->handleReceive(std::string(line_buf, line_pos));
                        line_pos = 0;
                    }
                    else {
                        // Prevent overflow
                        if (line_pos < LINE_BUF_SIZE - 1) {
                            line_buf[line_pos++] = c;
                        } else {
                            // overflow -> reset buffer
                            ESP_LOGW(TAG_GPS_READER, "Line buffer overflow, resetting");
                            line_pos = 0;
                        }
                    }
                }
                break;
            }

            case UART_BREAK:
                ESP_LOGW(TAG_GPS_READER, "UART BREAK detected");
                break;

            case UART_FIFO_OVF:
                ESP_LOGW(TAG_GPS_READER, "HW FIFO overflow");
                uart_flush_input(gps_reader->uartNum);
                xQueueReset(uart_queue);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG_GPS_READER, "Ring buffer full");
                uart_flush_input(gps_reader->uartNum);
                xQueueReset(uart_queue);
                break;

            default:
                break;
            }
        }
    }
}

GPS_Reader::GPS_Reader(gpio_num_t rx_pin, gpio_num_t tx_pin, uart_port_t uart_num) {
    this->rxPin = rx_pin;
    this->txPin = tx_pin;
    this->uartNum = uart_num;
    lastUpdateTime = time(nullptr);;
}

GPS_Reader::~GPS_Reader() {
    // Clean up UART driver and event queue
    uart_driver_delete(uartNum);
    if (uart_queue) {
        vQueueDelete(uart_queue);
    }
}

void GPS_Reader::begin() {

    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(uartNum, uart_buffer_size * 2, 0, 10, &uart_queue, 0));
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uartNum, &uart_config));

    // Set UART pins(TX: IO4, RX: IO5, RTS: UNUSED, CTS: UNUSED)
    ESP_ERROR_CHECK(uart_set_pin(uartNum, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(gps_readerTask, "gps_readerTask", 4096, this, 10, nullptr);
    ESP_LOGI(TAG_GPS_READER, "GPS Reader started on UART%d (RX: GPIO%d, TX: GPIO%d)", uartNum, rxPin, txPin);
}

void GPS_Reader::handleReceive(const std::string& line) {
    ESP_LOGD(TAG_GPS_READER, "Received GPS data: %s", line.c_str());

    switch (minmea_sentence_id(line.c_str(), false)) {
    case MINMEA_SENTENCE_RMC: {
        minmea_sentence_rmc frame{};
        if (minmea_parse_rmc(&frame, line.c_str())) {
            this->lastRMC = frame;
            ESP_LOGD(TAG_GPS_READER, "$xxRMC: raw coordinates and speed: (%d/%d,%d/%d) %d/%d\n",
                    frame.latitude.value, frame.latitude.scale,
                    frame.longitude.value, frame.longitude.scale,
                    frame.speed.value, frame.speed.scale);
            ESP_LOGD(TAG_GPS_READER, "$xxRMC fixed-point coordinates and speed scaled to three decimal places: (%d,%d) %d\n",
                    minmea_rescale(&frame.latitude, 1000),
                    minmea_rescale(&frame.longitude, 1000),
                    minmea_rescale(&frame.speed, 1000));
            ESP_LOGD(TAG_GPS_READER, "$xxRMC floating point degree coordinates and speed: (%f,%f) %f\n",
                    minmea_tocoord(&frame.latitude),
                    minmea_tocoord(&frame.longitude),
                    minmea_tofloat(&frame.speed));
        }
        else {
            ESP_LOGW(TAG_GPS_READER, "$xxRMC sentence is not parsed\n");
        }
    } break;

    case MINMEA_SENTENCE_GGA: {
        minmea_sentence_gga frame{};
        if (minmea_parse_gga(&frame, line.c_str())) {
            this->lastGGA = frame;
            ESP_LOGD(TAG_GPS_READER, "$xxGGA: fix quality: %d\n", frame.fix_quality);
        }
        else {
            ESP_LOGW(TAG_GPS_READER, "$xxGGA sentence is not parsed\n");
        }
    } break;

    case MINMEA_SENTENCE_GST: {
        minmea_sentence_gst frame{};
        if (minmea_parse_gst(&frame, line.c_str())) {
            this->lastGST = frame;
            ESP_LOGD(TAG_GPS_READER, "$xxGST: raw latitude,longitude and altitude error deviation: (%d/%d,%d/%d,%d/%d)\n",
                    frame.latitude_error_deviation.value, frame.latitude_error_deviation.scale,
                    frame.longitude_error_deviation.value, frame.longitude_error_deviation.scale,
                    frame.altitude_error_deviation.value, frame.altitude_error_deviation.scale);
            ESP_LOGD(TAG_GPS_READER, "$xxGST fixed point latitude,longitude and altitude error deviation"
                   " scaled to one decimal place: (%d,%d,%d)\n",
                    minmea_rescale(&frame.latitude_error_deviation, 10),
                    minmea_rescale(&frame.longitude_error_deviation, 10),
                    minmea_rescale(&frame.altitude_error_deviation, 10));
            ESP_LOGD(TAG_GPS_READER, "$xxGST floating point degree latitude, longitude and altitude error deviation: (%f,%f,%f)",
                    minmea_tofloat(&frame.latitude_error_deviation),
                    minmea_tofloat(&frame.longitude_error_deviation),
                    minmea_tofloat(&frame.altitude_error_deviation));
        }
        else {
            ESP_LOGW(TAG_GPS_READER, "$xxGST sentence is not parsed\n");
        }
    } break;

    case MINMEA_SENTENCE_GSV: {
        minmea_sentence_gsv frame{};
        if (minmea_parse_gsv(&frame, line.c_str())) {
            this->lastGSV = frame;
            ESP_LOGD(TAG_GPS_READER, "$xxGSV: message %d of %d\n", frame.msg_nr, frame.total_msgs);
            ESP_LOGD(TAG_GPS_READER, "$xxGSV: satellites in view: %d\n", frame.total_sats);
            for (int i = 0; i < 4; i++)
                ESP_LOGI(TAG_GPS_READER, "$xxGSV: sat nr %d, elevation: %d, azimuth: %d, snr: %f dbm\n",
                    frame.sats[i].nr,
                    frame.sats[i].elevation,
                    frame.sats[i].azimuth,
                    minmea_tofloat(&frame.sats[i].snr));
        }
        else {
            ESP_LOGW(TAG_GPS_READER, "$xxGSV sentence is not parsed\n");
        }
    } break;

    case MINMEA_SENTENCE_VTG: {
        minmea_sentence_vtg frame{};
        if (minmea_parse_vtg(&frame, line.c_str())) {
            this->lastVTG = frame;
            ESP_LOGD(TAG_GPS_READER, "$xxVTG: true track degrees = %f\n",
                   minmea_tofloat(&frame.true_track_degrees));
            ESP_LOGD(TAG_GPS_READER, "        magnetic track degrees = %f\n",
                   minmea_tofloat(&frame.magnetic_track_degrees));
            ESP_LOGD(TAG_GPS_READER, "        speed knots = %f\n",
                    minmea_tofloat(&frame.speed_knots));
            ESP_LOGD(TAG_GPS_READER, "        speed kph = %f\n",
                    minmea_tofloat(&frame.speed_kph));
        }
        else {
            ESP_LOGW(TAG_GPS_READER, "$xxVTG sentence is not parsed\n");
        }
    } break;

    case MINMEA_SENTENCE_ZDA: {
        minmea_sentence_zda frame{};
        if (minmea_parse_zda(&frame, line.c_str())) {
            this->lastZDA = frame;
            ESP_LOGD(TAG_GPS_READER, "$xxZDA: %d:%d:%d %02d.%02d.%d UTC%+03d:%02d\n",
                   frame.time.hours,
                   frame.time.minutes,
                   frame.time.seconds,
                   frame.date.day,
                   frame.date.month,
                   frame.date.year,
                   frame.hour_offset,
                   frame.minute_offset);
        }
        else {
            ESP_LOGW(TAG_GPS_READER, "$xxZDA sentence is not parsed\n");
        }
    } break;

    case MINMEA_INVALID: {
        ESP_LOGW(TAG_GPS_READER, "$xxxxx sentence is not valid\n");
    } break;

    default: {
        ESP_LOGW(TAG_GPS_READER, "$xxxxx sentence is not parsed\n");
    } break;
    }

    if (lastRMC.valid) {
        lastUpdateTime = time(nullptr);
    }
}


void GPS_Reader::getCurrentCoordinates(float& x, float& y, float& z) {
    x = minmea_tocoord(&lastRMC.latitude);
    y = minmea_tocoord(&lastRMC.longitude);
    z = minmea_tofloat(&lastGGA.altitude);
}

float GPS_Reader::getCurrentSpeed() {
    return minmea_tofloat(&lastRMC.speed);
}

tm GPS_Reader::getCurrentTime() const {
    tm currentTime{};
    currentTime.tm_year = lastZDA.date.year - 1900; // tm_year is years since 1900
    currentTime.tm_mon = lastZDA.date.month - 1;
    currentTime.tm_mday = lastZDA.date.day;
    currentTime.tm_hour = lastZDA.time.hours;
    currentTime.tm_min = lastZDA.time.minutes;
    currentTime.tm_sec = lastZDA.time.seconds;
    currentTime.tm_isdst = -1; // GPS time is always UTC+0
    return currentTime;
}


bool GPS_Reader::available() const {
    return lastUpdateTime - time(nullptr) < 10000;
}
