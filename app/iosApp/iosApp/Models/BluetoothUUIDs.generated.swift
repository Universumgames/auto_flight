//
// GENERATED FILE - DO NOT EDIT BY HAND.
// Source of truth: shared_components/flight_com/ble_protocol.json
// (topics mirror metadata.packetTypes in shared_components/flight_com/packets.jtd.json)
// Regenerate with shared_components/flight_com/gen_ble_protocol.py
//
// Mirrors BLETopics.generated.hpp (generated into flight_com's build dir).
//

import CoreBluetooth
import SharedLogic

let ServiceUUID = CBUUID(string: "ABF0")

enum BLEProtocolConstants {
    static let fragmentHeaderSize = 4
    static let mfgCompanyID: UInt16 = 0xFFFF
}

enum NotifyByte: UInt8, CaseIterable, Identifiable {
    case BLE_SENSOR_UPDATE = 0x10
    case BLE_POSITION = 0x11
    case BLE_COMPONENT_STATUS = 0x12
    /// the planned route by the plane to cover a specified area
    case BLE_PLANNED_ROUTE = 0x30
    /// the area the plane has to cover
    case BLE_PLANNED_AREA = 0x31
    /// the complete history of the plane's route since takeoff
    case BLE_ROUTE_HISTORY = 0x32
    /// requesting the complete history of the planes route since takeoff, use with caution
    case BLE_ROUTE_HISTORY_REQUEST = 0x33
    /// confirmation of the planned route by the base station
    case BLE_PLANNED_ROUTE_CONFIRMATION = 0x34
    /// current information about the route the plane is flying
    case BLE_FLIGHT_DATA = 0x40
    /// requesting a specific type of data from the plane, use with caution to avoid flooding the plane with requests
    case BLE_DATA_REQUEST = 0xFF

    var id: UInt8 { rawValue }

    var characteristic: CharacteristicUUID {
        switch self {
            case .BLE_SENSOR_UPDATE: return .sensorUpdate
            case .BLE_POSITION: return .position
            case .BLE_COMPONENT_STATUS: return .componentStatus
            case .BLE_PLANNED_ROUTE: return .plannedRoute
            case .BLE_PLANNED_AREA: return .plannedArea
            case .BLE_ROUTE_HISTORY: return .routeHistory
            case .BLE_ROUTE_HISTORY_REQUEST: return .routeHistoryRequest
            case .BLE_PLANNED_ROUTE_CONFIRMATION: return .plannedRouteConfirmation
            case .BLE_FLIGHT_DATA: return .flightData
            case .BLE_DATA_REQUEST: return .dataRequest
        }
    }
}

enum CharacteristicUUID: String, CaseIterable, Identifiable {
    case sensorUpdate = "AB10"
    case position = "AB11"
    case componentStatus = "AB12"
    /// the planned route by the plane to cover a specified area
    case plannedRoute = "AB30"
    /// the area the plane has to cover
    case plannedArea = "AB31"
    /// the complete history of the plane's route since takeoff
    case routeHistory = "AB32"
    /// requesting the complete history of the planes route since takeoff, use with caution
    case routeHistoryRequest = "AB33"
    /// confirmation of the planned route by the base station
    case plannedRouteConfirmation = "AB34"
    /// current information about the route the plane is flying
    case flightData = "AB40"
    /// requesting a specific type of data from the plane, use with caution to avoid flooding the plane with requests
    case dataRequest = "ABFF"
}

extension CharacteristicUUID {
    var uuid: CBUUID {
        return CBUUID(string: rawValue)
    }

    var id: CBUUID {
        uuid
    }

    /// The `wire.PacketType` carried by this characteristic.
    var packetType: wire.PacketType {
        notifyByte.packetType
    }

    /// The topic byte this characteristic belongs to.
    var notifyByte: NotifyByte {
        switch self {
            case .sensorUpdate: return .BLE_SENSOR_UPDATE
            case .position: return .BLE_POSITION
            case .componentStatus: return .BLE_COMPONENT_STATUS
            case .plannedRoute: return .BLE_PLANNED_ROUTE
            case .plannedArea: return .BLE_PLANNED_AREA
            case .routeHistory: return .BLE_ROUTE_HISTORY
            case .routeHistoryRequest: return .BLE_ROUTE_HISTORY_REQUEST
            case .plannedRouteConfirmation: return .BLE_PLANNED_ROUTE_CONFIRMATION
            case .flightData: return .BLE_FLIGHT_DATA
            case .dataRequest: return .BLE_DATA_REQUEST
        }
    }
}

extension NotifyByte {
    /// The `wire.PacketType` carried by this topic.
    var packetType: wire.PacketType {
        switch self {
            case .BLE_SENSOR_UPDATE: return .SENSOR_UPDATE
            case .BLE_POSITION: return .POSITION
            case .BLE_COMPONENT_STATUS: return .COMPONENT_STATUS
            case .BLE_PLANNED_ROUTE: return .PLANNED_ROUTE
            case .BLE_PLANNED_AREA: return .PLANNED_AREA
            case .BLE_ROUTE_HISTORY: return .ROUTE_HISTORY
            case .BLE_ROUTE_HISTORY_REQUEST: return .ROUTE_HISTORY_REQUEST
            case .BLE_PLANNED_ROUTE_CONFIRMATION: return .PLANNED_ROUTE_CONFIRMATION
            case .BLE_FLIGHT_DATA: return .FLIGHT_DATA
            case .BLE_DATA_REQUEST: return .DATA_REQUEST
        }
    }
}

/// Exhaustive on purpose: if the KMP wire.PacketType (FrontendPackets.kt) drifts from the
/// C++ enum, these stop compiling.
extension wire.PacketType {
    /// The topic carrying this packet type.
    var notifyByte: NotifyByte {
        switch self {
            case .SENSOR_UPDATE: return .BLE_SENSOR_UPDATE
            case .POSITION: return .BLE_POSITION
            case .COMPONENT_STATUS: return .BLE_COMPONENT_STATUS
            case .PLANNED_ROUTE: return .BLE_PLANNED_ROUTE
            case .PLANNED_AREA: return .BLE_PLANNED_AREA
            case .ROUTE_HISTORY: return .BLE_ROUTE_HISTORY
            case .ROUTE_HISTORY_REQUEST: return .BLE_ROUTE_HISTORY_REQUEST
            case .PLANNED_ROUTE_CONFIRMATION: return .BLE_PLANNED_ROUTE_CONFIRMATION
            case .FLIGHT_DATA: return .BLE_FLIGHT_DATA
            case .DATA_REQUEST: return .BLE_DATA_REQUEST
        }
    }

    /// The characteristic carrying this packet type.
    var characteristic: CharacteristicUUID {
        notifyByte.characteristic
    }
}
