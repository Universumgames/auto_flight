#include "CameraCommunication.hpp"

#include <cstring>

#ifndef NATIVE_BUILD
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace CameraCommunication{

    bool sendScanRequest(const ScanRequest& request, i2c_bus_device_handle_t devHandle) {
        esp_err_t err = i2c_bus_write_bytes(devHandle, SCAN_REQUEST, sizeof(ScanRequest), reinterpret_cast<const uint8_t*>(&request));
        return err == ESP_OK;
    }

    std::optional<ScanResult> receiveScanResult(i2c_bus_device_handle_t devHandle) {
        uint8_t ready;
        esp_err_t err = i2c_bus_read_byte(devHandle, SCAN_RESULT_READY, &ready);
        if (err != ESP_OK || !ready) {
            return std::nullopt;
        }
        ScanResult result;
        err = i2c_bus_read_bytes(devHandle, SCAN_RESULT_DATA, sizeof(ScanResult), reinterpret_cast<uint8_t*>(&result));
        if (err != ESP_OK) {
            return std::nullopt;
        }
        return result;
    }

    std::optional<ScanRequest> decodeScanRequest(const uint8_t* buffer, const size_t length) {
        if (length < sizeof(ScanRequest)) {
            return std::nullopt;
        }
        ScanRequest request;
        memcpy(&request, buffer, sizeof(ScanRequest));
        return request;
    }

    bool sendScanResult(const ScanResult& result, i2c_slave_dev_handle_t slaveHandle, int timeoutMs) {
        uint32_t written = 0;
        esp_err_t err = i2c_slave_write(slaveHandle, reinterpret_cast<const uint8_t*>(&result), sizeof(ScanResult), &written, timeoutMs);
        return err == ESP_OK && written == sizeof(ScanResult);
    }

    std::optional<CNNClassifierMetadata> readClassifierMetadata(i2c_bus_device_handle_t devHandle) {
        // Wire format: [length:u8][name\0][label\0]...  (length covers the strings only)
        uint8_t length = 0;
        esp_err_t err = i2c_bus_read_byte(devHandle, PROBE, &length);
        if (err != ESP_OK || length == 0) {
            return std::nullopt;
        }

        // NULL_I2C_MEM_ADDR skips the register write, so this is a plain read continuing from the slave's tx buffer
        std::vector<uint8_t> data(length);
        err = i2c_bus_read_bytes(devHandle, NULL_I2C_MEM_ADDR, length, data.data());
        if (err != ESP_OK) {
            return std::nullopt;
        }

        CNNClassifierMetadata metadata;
        try {
            nlohmann::json::from_cbor(data.begin(), data.end()).get_to(metadata);
        } catch (const nlohmann::json::exception& e) {
            ESP_LOGE("CameraCommunication", "Malformed classifier metadata: %s", e.what());
            return std::nullopt;
        }
        return metadata;
    }

    bool sendCNNClassifierMetadata(const CNNClassifierMetadata& metadata, i2c_slave_dev_handle_t slaveHandle, int timeoutMs) {
        nlohmann::json j = metadata;
        std::vector<uint8_t> cbor = nlohmann::json::to_cbor(j);
        size_t payloadLength = cbor.size();
        if (payloadLength > UINT8_MAX) {
            ESP_LOGE("CameraCommunication", "Classifier metadata too large: %u bytes", static_cast<unsigned>(payloadLength));
            return false;
        }
        cbor.insert(cbor.begin(), static_cast<uint8_t>(payloadLength));

        uint32_t written = 0;
        esp_err_t err = i2c_slave_write(slaveHandle, cbor.data(), cbor.size(), &written, timeoutMs);
        return err == ESP_OK && written == cbor.size();
    }
}
#endif