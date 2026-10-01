// GENERATED FILE - DO NOT EDIT BY HAND.
// Source of truth: base_station/components/frontend_bluetooth/ble_protocol.json
// (topics mirror enum PacketType in base_station/shared_components/flight_com/packets/base.hpp)
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
    BLE_SENSOR_UPDATE(0x10u, "AB10"),
    BLE_POSITION(0x11u, "AB11"),
    BLE_COMPONENT_STATUS(0x12u, "AB12"),
    /// the planned route by the plane to cover a specified area
    BLE_PLANNED_ROUTE(0x30u, "AB30"),
    /// the area the plane has to cover
    BLE_PLANNED_AREA(0x31u, "AB31"),
    /// the complete history of the plane's route since takeoff
    BLE_ROUTE_HISTORY(0x32u, "AB32"),
    /// requesting the complete history of the planes route since takeoff, use with caution
    BLE_ROUTE_HISTORY_REQUEST(0x33u, "AB33"),
    /// confirmation of the planned route by the base station
    BLE_PLANNED_ROUTE_CONFIRMATION(0x34u, "AB34"),
    ;

    /** The [PacketType] carried by this topic. */
    val packetType: PacketType
        get() = when (this) {
            BLE_SENSOR_UPDATE -> PacketType.SENSOR_UPDATE
            BLE_POSITION -> PacketType.POSITION
            BLE_COMPONENT_STATUS -> PacketType.COMPONENT_STATUS
            BLE_PLANNED_ROUTE -> PacketType.PLANNED_ROUTE
            BLE_PLANNED_AREA -> PacketType.PLANNED_AREA
            BLE_ROUTE_HISTORY -> PacketType.ROUTE_HISTORY
            BLE_ROUTE_HISTORY_REQUEST -> PacketType.ROUTE_HISTORY_REQUEST
            BLE_PLANNED_ROUTE_CONFIRMATION -> PacketType.PLANNED_ROUTE_CONFIRMATION
        }
}

/**
 * The [BleTopic] carrying this packet type. Exhaustive on purpose: if the hand-written
 * [PacketType] in FrontendPackets.kt drifts from the C++ enum, this stops compiling.
 */
val PacketType.bleTopic: BleTopic
    get() = when (this) {
        PacketType.SENSOR_UPDATE -> BleTopic.BLE_SENSOR_UPDATE
        PacketType.POSITION -> BleTopic.BLE_POSITION
        PacketType.COMPONENT_STATUS -> BleTopic.BLE_COMPONENT_STATUS
        PacketType.PLANNED_ROUTE -> BleTopic.BLE_PLANNED_ROUTE
        PacketType.PLANNED_AREA -> BleTopic.BLE_PLANNED_AREA
        PacketType.ROUTE_HISTORY -> BleTopic.BLE_ROUTE_HISTORY
        PacketType.ROUTE_HISTORY_REQUEST -> BleTopic.BLE_ROUTE_HISTORY_REQUEST
        PacketType.PLANNED_ROUTE_CONFIRMATION -> BleTopic.BLE_PLANNED_ROUTE_CONFIRMATION
    }
