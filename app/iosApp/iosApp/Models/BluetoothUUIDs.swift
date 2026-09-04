//
//  BluetoothUUIDs.swift
//  iosApp
//
//  Created by Tom Arlt on 28.08.26.
//

import CoreBluetooth

let ServiceUUID = CBUUID(string: "ABF0")

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
    /*
     BLE_UUID16_INIT(0xABF1), // BLE_TOPIC_ALL
     BLE_UUID16_INIT(0xABF2), // BLE_TOPIC_FLIGHT_UPDATE
     BLE_UUID16_INIT(0xABF3), // BLE_TOPIC_CONNECTION_UPDATE
     BLE_UUID16_INIT(0xABF4), // BLE_TOPIC_SENSOR_DATA
     BLE_UUID16_INIT(0xABF5), // BLE_TOPIC_AREA_DEFINE
     BLE_UUID16_INIT(0xABF6), // BLE_TOPIC_PLANNED_ROUTE
     BLE_UUID16_INIT(0xABF7)  // BLE_TOPIC_BATTERY_STATUS
     */

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
