//
//  BluetoothUUIDs.swift
//  iosApp
//
//  Created by Tom Arlt on 28.08.26.
//
//  ServiceUUID, NotifyByte and CharacteristicUUID now live in the generated
//  BluetoothUUIDs.generated.swift (see that file's header for how to regenerate it).

import CoreBluetooth

/// A base station discovered while scanning, together with the firmware build date
/// parsed from its advertisement — available before a connection is ever made. See
/// `BluetoothManager::populateBuildVersionMfgData` in
/// base_station/components/frontend_bluetooth/BluetoothManager.cpp for the wire format
/// this mirrors.
struct DiscoveredBaseStation: Identifiable {
    let peripheral: CBPeripheral
    let buildDate: Date?
    /// Updated on every `didDiscover` callback for this peripheral — used to drop it
    /// from `discoveredBaseStations` once its advertisements stop arriving.
    var lastSeen: Date = Date()

    var id: UUID { peripheral.identifier }
    var name: String { peripheral.name ?? "Unknown Device" }
}

/// Parses the firmware build date out of a base station's manufacturer-specific
/// advertisement data (company ID + little-endian Unix-epoch uint32), or nil if the
/// data is absent, too short, or tagged with a different company ID.
func parseBaseStationBuildDate(fromAdvertisementData advertisementData: [String: Any]) -> Date? {
    guard let mfgData = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data,
          mfgData.count >= 6 else { return nil }
    let bytes = [UInt8](mfgData)
    let companyID = UInt16(bytes[0]) | (UInt16(bytes[1]) << 8)
    guard companyID == BLEProtocolConstants.mfgCompanyID else { return nil }
    let epoch = UInt32(bytes[2])
        | (UInt32(bytes[3]) << 8)
        | (UInt32(bytes[4]) << 16)
        | (UInt32(bytes[5]) << 24)
    return Date(timeIntervalSince1970: TimeInterval(epoch))
}
