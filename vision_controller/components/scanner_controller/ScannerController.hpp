#pragma once

#include <optional>

#include "Camera.hpp"
#include "driver/i2c_slave.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "CameraCommunication.hpp"
#include "CNN.hpp"

class ScannerControllerClass {
private:
    ScannerControllerClass() = default;

public:
    ~ScannerControllerClass() = delete;

public:
    static ScannerControllerClass* getInstancePtr();
    static ScannerControllerClass& getInstance();

public:
    /**
     * Initialize the I2C slave bus answering to CAMERA_CONTROLLER_I2C_ADDR and start the task answering the master.
     * Must be called before any other method is used.
     */
    void init();

    /**
     * Wait for the master to send a scan request. Only the latest unhandled request is kept.
     * Must not be called from ISR context.
     * @param timeoutMs time to wait for a request, -1 waits forever
     * @return the request, std::nullopt on timeout
     */
    [[nodiscard]] std::optional<CameraCommunication::ScanRequest> waitForScanRequest(int timeoutMs = -1) const;

    /**
     * Publish a scan result. The master sees it as ready until it read it via SCAN_RESULT_DATA.
     * @param result the result to hand to the master
     */
    void setScanResult(const std::optional<CameraCommunication::ScanResult>& result);

    /**
     * Set the classifier metadata the master reads via PROBE
     * @param metadata the metadata to hand to the master
     */
    void setClassifierMetadata(const CameraCommunication::CNNClassifierMetadata& metadata);

private:
    i2c_slave_dev_handle_t slaveHandle = nullptr;

    QueueHandle_t scanRequestQueue = nullptr;
    SemaphoreHandle_t stateMutex = nullptr;

    // Guarded by stateMutex
    std::optional<CameraCommunication::ScanResult> scanResult;
    std::optional<CameraCommunication::CNNClassifierMetadata> classifierMetadata;

    Camera* camera = nullptr;
    CNN* classifier = nullptr;

    void handleReceive(const uint8_t* data, size_t length);
    void handleRequest(uint8_t command);

    static void scanLoop(void* arg);
};

extern ScannerControllerClass& ScannerController;
