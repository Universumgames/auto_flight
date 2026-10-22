//
// Created by Tom Arlt on 22.10.26.
//

#include "CameraCommunication.hpp"

#include <cstring>

#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace CameraCommunication{
    struct ReceivedPacket {
        uint32_t length;
        uint8_t data[1 + sizeof(ScanRequest)];
    };

    static i2c_slave_dev_handle_t registeredSlave = nullptr;
    static QueueHandle_t receiveQueue = nullptr;

    static bool IRAM_ATTR onSlaveReceive(i2c_slave_dev_handle_t, const i2c_slave_rx_done_event_data_t* evtData, void* arg) {
        auto queue = static_cast<QueueHandle_t>(arg);
        ReceivedPacket packet{};
        packet.length = evtData->length < sizeof(packet.data) ? evtData->length : sizeof(packet.data);
        memcpy(packet.data, evtData->buffer, packet.length);

        BaseType_t higherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(queue, &packet, &higherPriorityTaskWoken);
        return higherPriorityTaskWoken == pdTRUE;
    }

    static bool registerReceiveCallback(i2c_slave_dev_handle_t slaveHandle) {
        if (registeredSlave == slaveHandle) {
            return true;
        }

        if (!receiveQueue) {
            receiveQueue = xQueueCreate(4, sizeof(ReceivedPacket));
            if (!receiveQueue) {
                ESP_LOGE("CameraCommunication", "Failed to create receive queue");
                return false;
            }
        }

        i2c_slave_event_callbacks_t callbacks = {
            .on_request = nullptr,
            .on_receive = onSlaveReceive,
        };
        esp_err_t err = i2c_slave_register_event_callbacks(slaveHandle, &callbacks, receiveQueue);
        if (err != ESP_OK) {
            ESP_LOGE("CameraCommunication", "Failed to register receive callback: %s", esp_err_to_name(err));
            return false;
        }

        registeredSlave = slaveHandle;
        return true;
    }

    bool sendScanRequest(const ScanRequest& request, i2c_bus_device_handle_t devHandle) {
        esp_err_t err = i2c_bus_write_bytes(devHandle, SCAN_REQUEST, sizeof(ScanRequest), reinterpret_cast<const uint8_t*>(&request));
        return err == ESP_OK;
    }

    std::optional<ScanResult> receiveScanResult(i2c_bus_device_handle_t devHandle) {
        ScanResult result;
        esp_err_t err = i2c_bus_read_bytes(devHandle, SCAN_RESULT_DATA, sizeof(ScanResult), reinterpret_cast<uint8_t*>(&result));
        if (err != ESP_OK) {
            return std::nullopt;
        }
        return result;
    }

    std::optional<ScanRequest> receiveScanRequest(i2c_slave_dev_handle_t slaveHandle, int timeoutMs) {
        if (!registerReceiveCallback(slaveHandle)) {
            return std::nullopt;
        }

        const TickType_t start = xTaskGetTickCount();
        const TickType_t timeout = timeoutMs < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);

        ReceivedPacket packet;
        while (true) {
            TickType_t remaining = portMAX_DELAY;
            if (timeoutMs >= 0) {
                const TickType_t elapsed = xTaskGetTickCount() - start;
                if (elapsed >= timeout) {
                    return std::nullopt;
                }
                remaining = timeout - elapsed;
            }

            if (xQueueReceive(receiveQueue, &packet, remaining) != pdTRUE) {
                return std::nullopt;
            }

            if (packet.length >= 1 + sizeof(ScanRequest) && packet.data[0] == SCAN_REQUEST) {
                ScanRequest request;
                memcpy(&request, packet.data + 1, sizeof(ScanRequest));
                return request;
            }
        }
    }

    bool sendScanResult(const ScanResult& result, i2c_slave_dev_handle_t slaveHandle, int timeoutMs) {
        uint32_t written = 0;
        esp_err_t err = i2c_slave_write(slaveHandle, reinterpret_cast<const uint8_t*>(&result), sizeof(ScanResult), &written, timeoutMs);
        return err == ESP_OK && written == sizeof(ScanResult);
    }
}
