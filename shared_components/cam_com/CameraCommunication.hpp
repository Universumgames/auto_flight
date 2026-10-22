#pragma once
#include <cstdint>
#include <optional>

#include "FlightTypes.generated.hpp"

#ifndef NATIVE_BUILD
#include "i2c_bus.h"
#include "driver/i2c_slave.h"
#endif

#define CAMERA_CONTROLLER_I2C_ADDR 0x44

namespace CameraCommunication{
    enum ScanI2CCommand : uint8_t {
        EMPTY = 0x00,
        SCAN_REQUEST = 0x01,
        SCAN_RESULT_READY = 0x02,
        SCAN_RESULT_DATA = 0x03
    };

    struct ScanRequest {
        Coordinate position;
        float heightAboveGround{};
    };

    struct ScanResult {
        uint8_t resultId = 0;
        Coordinate area[4]; // The four corners of the scanned area, in order
    };

    // Master side

#ifndef NATIVE_BUILD
    /**
     * Send a scan request to the slave, written as [SCAN_REQUEST][ScanRequest]
     * @param request the request to send
     * @param devHandle i2c_bus device of the slave
     * @return true on success, false otherwise
     */
    bool sendScanRequest(const ScanRequest& request, i2c_bus_device_handle_t devHandle);

    /**
     * Request the scan result from the slave by writing SCAN_RESULT_DATA and reading back a ScanResult
     * @param devHandle i2c_bus device of the slave
     * @return the result, std::nullopt if the transaction failed
     */
    std::optional<ScanResult> receiveScanResult(i2c_bus_device_handle_t devHandle);

    // Slave side

    /**
     * Wait for the master to send a scan request. On first use this registers the slave's on_receive callback,
     * replacing any previously registered one. Received writes that are not a scan request are discarded.
     * Must not be called from ISR context.
     * @param slaveHandle slave device handle
     * @param timeoutMs time to wait for a request, -1 waits forever
     * @return the request, std::nullopt on timeout or if the callback could not be registered
     */
    std::optional<ScanRequest> receiveScanRequest(i2c_slave_dev_handle_t slaveHandle, int timeoutMs = -1);

    /**
     * Write the scan result into the slave's transmit buffer, to be read by the master after it sent SCAN_RESULT_DATA.
     * Must not be called from ISR context.
     * @param result the result to send
     * @param slaveHandle slave device handle
     * @param timeoutMs time to wait for space in the transmit buffer, -1 waits forever
     * @return true if the full result was written, false otherwise
     */
    bool sendScanResult(const ScanResult& result, i2c_slave_dev_handle_t slaveHandle, int timeoutMs = 100);
#endif
}
