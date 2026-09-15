//
// GENERATED FILE - DO NOT EDIT BY HAND.
// Source of truth: base_station/components/frontend_bluetooth/ble_protocol.json
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
    case BLE_TOPIC_ALL = 0x00
    case BLE_TOPIC_FLIGHT_UPDATE = 0x02
    case BLE_TOPIC_CONNECTION_UPDATE = 0x03
    case BLE_TOPIC_SENSOR_DATA = 0x04
    case BLE_TOPIC_AREA_DEFINE = 0x05
    case BLE_TOPIC_PLANNED_ROUTE = 0x06
    case BLE_TOPIC_BATTERY_STATUS = 0x07

    var id: UInt8 { rawValue }

    var characteristic: CharacteristicUUID {
        switch self {
            case .BLE_TOPIC_ALL: return .allUpdate
            case .BLE_TOPIC_FLIGHT_UPDATE: return .flightUpdate
            case .BLE_TOPIC_CONNECTION_UPDATE: return .connectionUpdate
            case .BLE_TOPIC_SENSOR_DATA: return .sensorData
            case .BLE_TOPIC_AREA_DEFINE: return .areaDefine
            case .BLE_TOPIC_PLANNED_ROUTE: return .plannedRoute
            case .BLE_TOPIC_BATTERY_STATUS: return .batteryStatus
        }
    }
}

enum CharacteristicUUID: String, CaseIterable, Identifiable {
    case allUpdate = "ABF1"
    case flightUpdate = "ABF2"
    case connectionUpdate = "ABF3"
    case sensorData = "ABF4"
    case areaDefine = "ABF5"
    case plannedRoute = "ABF6"
    case batteryStatus = "ABF7"
}

extension CharacteristicUUID {
    var uuid: CBUUID {
        return CBUUID(string: rawValue)
    }

    var id: CBUUID {
        uuid
    }
}
