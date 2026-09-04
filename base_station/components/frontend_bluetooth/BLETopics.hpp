#pragma once
#include <cstddef>
#include <cstdint>

#include "host/ble_uuid.h"

namespace BLETopics {
    enum NotifyByte : uint8_t {
        BLE_TOPIC_ALL = 0x00,
        BLE_TOPIC_FLIGHT_UPDATE = 0x02,
        BLE_TOPIC_CONNECTION_UPDATE = 0x03,
        BLE_TOPIC_SENSOR_DATA = 0x04,
        BLE_TOPIC_AREA_DEFINE = 0x05,
        BLE_TOPIC_PLANNED_ROUTE = 0x06,
        BLE_TOPIC_BATTERY_STATUS = 0x07,
    };

    inline constexpr NotifyByte ALL_NOTIFICATIONS[] = {
        BLE_TOPIC_ALL,
        BLE_TOPIC_FLIGHT_UPDATE,
        BLE_TOPIC_CONNECTION_UPDATE,
        BLE_TOPIC_SENSOR_DATA,
        BLE_TOPIC_AREA_DEFINE,
        BLE_TOPIC_PLANNED_ROUTE,
        BLE_TOPIC_BATTERY_STATUS
    };

    inline constexpr size_t ALL_NOTIFICATIONS_SIZE = std::size(ALL_NOTIFICATIONS);

    // UUIDs, in the same order as ALL_NOTIFICATIONS (looked up by position, not by enum value).
    inline constexpr ble_uuid16_t TOPIC_UUIDS[ALL_NOTIFICATIONS_SIZE] = {
        BLE_UUID16_INIT(0xABF1), // BLE_TOPIC_ALL
        BLE_UUID16_INIT(0xABF2), // BLE_TOPIC_FLIGHT_UPDATE
        BLE_UUID16_INIT(0xABF3), // BLE_TOPIC_CONNECTION_UPDATE
        BLE_UUID16_INIT(0xABF4), // BLE_TOPIC_SENSOR_DATA
        BLE_UUID16_INIT(0xABF5), // BLE_TOPIC_AREA_DEFINE
        BLE_UUID16_INIT(0xABF6), // BLE_TOPIC_PLANNED_ROUTE
        BLE_UUID16_INIT(0xABF7)  // BLE_TOPIC_BATTERY_STATUS
    };

    /**
     * Look up the 16-bit UUID registered for a topic.
     * @return Pointer to the matching entry in TOPIC_UUIDS, or nullptr if the
     *         topic has no registered UUID (e.g. BLE_TOPIC_NONE or an unknown value).
     */
    inline const ble_uuid16_t* getUUIDForTopic(const NotifyByte topic) {
        for (size_t i = 0; i < ALL_NOTIFICATIONS_SIZE; ++i) {
            if (ALL_NOTIFICATIONS[i] == topic) {
                return &TOPIC_UUIDS[i];
            }
        }
        return nullptr; // Invalid/unregistered topic
    }
}
