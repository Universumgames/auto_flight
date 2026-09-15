// GENERATED FILE - DO NOT EDIT BY HAND.
// Source of truth: base_station/components/frontend_bluetooth/ble_protocol.json
// Regenerate with base_station/components/frontend_bluetooth/gen_ble_protocol.py
//
// STUB: not referenced by any code yet - the Android app currently talks to the
// base station over a websocket (see KtorFlightRepository.kt), not Bluetooth.
// This exists so the day Android/KMP grows a BLE transport, the wire contract is
// already generated from the same source of truth as the C++ and Swift sides
// instead of being hand-copied a third time. Regenerate with
// base_station/components/frontend_bluetooth/gen_ble_protocol.py --out-kotlin <path>
package de.universegame.auto_flight.app.wire

object BleProtocol {
    const val SERVICE_UUID: String = "ABF0"
    const val FRAGMENT_HEADER_SIZE: Int = 4
    const val MFG_COMPANY_ID: Int = 0xFFFF
    const val MFG_BUILD_EPOCH_FIELD_SIZE: Int = 4
    const val MFG_DATA_LEN: Int = 2 + MFG_BUILD_EPOCH_FIELD_SIZE
}

enum class BleTopic(val value: UByte, val characteristicUUID: String) {
    BLE_TOPIC_ALL(0x00u, "ABF1"),
    BLE_TOPIC_FLIGHT_UPDATE(0x02u, "ABF2"),
    BLE_TOPIC_CONNECTION_UPDATE(0x03u, "ABF3"),
    BLE_TOPIC_SENSOR_DATA(0x04u, "ABF4"),
    BLE_TOPIC_AREA_DEFINE(0x05u, "ABF5"),
    BLE_TOPIC_PLANNED_ROUTE(0x06u, "ABF6"),
    BLE_TOPIC_BATTERY_STATUS(0x07u, "ABF7"),
}
