//
// Created by Tom Arlt on 22.10.26.
//

#include "ScannerController.hpp"

#include <cstring>

#include "esp_log.h"
#include "freertos/task.h"

#include "scanner_i2c_manager.hpp"

using namespace CameraCommunication;

static const char* TAG_SCANNER_CONTROLLER = "ScannerController";

static ScannerControllerClass *scannerControllerClass = nullptr;

ScannerControllerClass& ScannerController = ScannerControllerClass::getInstance();

ScannerControllerClass* ScannerControllerClass::getInstancePtr() {
    if (!scannerControllerClass) {
        scannerControllerClass = new ScannerControllerClass();
    }
    return scannerControllerClass;
}

ScannerControllerClass& ScannerControllerClass::getInstance() {
    return *getInstancePtr();
}

void ScannerControllerClass::init() {
    slaveHandle = ScannerI2CManager::getSlaveBus(CAMERA_CONTROLLER_I2C_ADDR);
    if (!slaveHandle) {
        ESP_LOGE(TAG_SCANNER_CONTROLLER, "No I2C slave bus available");
        return;
    }

    scanRequestQueue = xQueueCreate(1, sizeof(ScanRequest));
    stateMutex = xSemaphoreCreateMutex();
    if (!scanRequestQueue || !stateMutex) {
        ESP_LOGE(TAG_SCANNER_CONTROLLER, "Failed to create queues");
        return;
    }

    const bool registered = ScannerI2CManager::registerSlaveCallbacks(
        [this](const uint8_t* data, size_t length) { handleReceive(data, length); },
        [this](uint8_t command) { handleRequest(command); });
    if (!registered) {
        ESP_LOGE(TAG_SCANNER_CONTROLLER, "Failed to register slave callbacks");
        return;
    }

    xTaskCreate(scanLoop, "scanner_scan", 4096, this, configMAX_PRIORITIES - 3, nullptr);

    camera = new Camera();
    if (camera->init() != ESP_OK) {
        ESP_LOGE(TAG_SCANNER_CONTROLLER, "Failed to initialize camera");
    }
    classifier = new CNN();
    if (classifier->init() != ESP_OK) {
        ESP_LOGE(TAG_SCANNER_CONTROLLER, "Failed to initialize CNN classifier");
    }
}

std::optional<ScanRequest> ScannerControllerClass::waitForScanRequest(int timeoutMs) const {
    ScanRequest request;
    const TickType_t timeout = timeoutMs < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
    if (xQueueReceive(scanRequestQueue, &request, timeout) != pdTRUE) {
        return std::nullopt;
    }
    return request;
}

void ScannerControllerClass::setScanResult(const std::optional<ScanResult>& result) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    scanResult = result;
    xSemaphoreGive(stateMutex);
}

void ScannerControllerClass::setClassifierMetadata(const CNNClassifierMetadata& metadata) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    classifierMetadata = metadata;
    xSemaphoreGive(stateMutex);
}

void ScannerControllerClass::handleReceive(const uint8_t* data, size_t length) {
    // A bare command byte only selects what the next read returns, that is already tracked by the I2C manager
    if (data[0] == SCAN_REQUEST) {
        if (length < 1 + sizeof(ScanRequest)) {
            ESP_LOGW(TAG_SCANNER_CONTROLLER, "Scan request too short: %u bytes", static_cast<unsigned>(length));
            return;
        }
        auto request = decodeScanRequest(data + 1, length - 1);
        xQueueOverwrite(scanRequestQueue, &request);
    }
}

void ScannerControllerClass::handleRequest(uint8_t command) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    switch (command) {
        case PROBE: {
            auto res = sendCNNClassifierMetadata(classifierMetadata.value_or(CNNClassifierMetadata{}), slaveHandle);
            if (!res) {
                ESP_LOGW(TAG_SCANNER_CONTROLLER, "Failed to send classifier metadata");
            }
            break;
        }
        case SCAN_RESULT_READY: {
            const uint8_t ready = scanResult.has_value();
            uint32_t written = 0;
            i2c_slave_write(slaveHandle, &ready, 1, &written, 100);
            break;
        }
        case SCAN_RESULT_DATA: {
            auto res = sendScanResult(scanResult.value_or(ScanResult{}), slaveHandle);
            if (!res) {
                ESP_LOGW(TAG_SCANNER_CONTROLLER, "Failed to send scan result");
            }
            break;
        }
        default:
            ESP_LOGW(TAG_SCANNER_CONTROLLER, "Read for unknown command 0x%02X", command);
            break;
    }
    xSemaphoreGive(stateMutex);
}

void ScannerControllerClass::scanLoop(void* arg) {
    const auto self = getInstancePtr();
    while (true) {
        auto request = self->waitForScanRequest();
        if (!request) {
            continue;
        }
        // TODO camera record image
        // TODO run scan on image
        // TODO update scan result
        self->setScanResult(std::nullopt);
    }
}
