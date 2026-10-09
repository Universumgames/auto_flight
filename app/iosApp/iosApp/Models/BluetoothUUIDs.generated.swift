//
// GENERATED FILE - DO NOT EDIT BY HAND.
// Source of truth: base_station/components/frontend_bluetooth/ble_protocol.json
// (topics mirror enum PacketType in base_station/shared_components/flight_com/packets/base.hpp)
// Regenerate with base_station/components/frontend_bluetooth/gen_ble_protocol.py
//
// Mirrors base_station/components/frontend_bluetooth/BLETopics.generated.hpp.
//

import CoreBluetooth

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
}

extension CharacteristicUUID {
    var uuid: CBUUID {
        return CBUUID(string: rawValue)
    }

    var id: CBUUID {
        uuid
    }
}
