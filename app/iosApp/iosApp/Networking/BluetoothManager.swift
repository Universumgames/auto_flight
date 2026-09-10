//
//  BluetoothManager.swift
//  iosApp
//
//  Created by Tom Arlt on 27.08.26.
//

import CoreBluetooth
import Foundation

enum BluetoothError: Error {
    case notConnected
    case characteristicNotFound
    case payloadTooLarge
    case mtuTooSmall
}

extension ConnectionManager: CBCentralManagerDelegate, CBPeripheralDelegate {
    // MARK: - Scanning

    func startBluetoothScan() {
        if centralManager == nil {
            centralManager = CBCentralManager(
                delegate: self,
                queue: .main,
                options: [CBCentralManagerOptionShowPowerAlertKey: true]
            )
        }
        guard let cm = centralManager, cm.state == .poweredOn else {
            pendingStartScan = true
            return
        }
        beginScan()
    }

    func stopBluetoothScan() {
        pendingStartScan = false
        isScanning = false
        centralManager?.stopScan()
    }

    // MARK: - Characteristic Operations

    /// One-shot read — returns the characteristic's current value, or nil on failure.
    /// Only for characteristics the base station still serves as a plain (unfragmented)
    /// GATT read, i.e. those under the 512-byte attribute limit — see `awaitNextUpdate`
    /// in ConnectionManager for characteristics served over the fragmented notify path.
    func readCharacteristic(_ char: CharacteristicUUID) async -> Data? {
        guard let characteristic = discoveredCharacteristics[char.uuid],
              let peripheral = connectedPeripheral else { return nil }
        return await withCheckedContinuation { continuation in
            pendingRawReads.insert(char.uuid)
            readCompletions[char.uuid] = { continuation.resume(returning: $0) }
            peripheral.readValue(for: characteristic)
        }
    }

    /// Write data to a characteristic, split into `BLEFragmentFraming`-framed chunks that
    /// fit the negotiated ATT MTU and reassembled server-side by `handleFragmentedWrite`
    /// (base_station/components/frontend_bluetooth/BluetoothManager.cpp) — mirroring the
    /// fragmentation already used for notifications. Uses write-with-response when
    /// available; otherwise write-without-response.
    func writeCharacteristic(_ char: CharacteristicUUID, data: Data) async throws {
        guard let characteristic = discoveredCharacteristics[char.uuid],
              let peripheral = connectedPeripheral else {
            throw BluetoothError.notConnected
        }
        guard data.count <= Int(UInt16.max) else {
            throw BluetoothError.payloadTooLarge
        }
        let type: CBCharacteristicWriteType = characteristic.properties.contains(.writeWithoutResponse) ? .withoutResponse : .withResponse
        let chunkSize = peripheral.maximumWriteValueLength(for: type) - BLEFragmentFraming.headerSize
        guard chunkSize > 0 else { throw BluetoothError.mtuTooSmall }

        let bytes = [UInt8](data)
        var offset = 0
        repeat {
            let thisChunkLen = min(chunkSize, bytes.count - offset)
            let chunk = Data(bytes[offset..<(offset + thisChunkLen)])
            let frame = BLEFragmentFraming.makeFrame(totalLength: bytes.count, offset: offset, chunk: chunk)
            try await writeFragment(frame, to: characteristic, type: type)
            offset += thisChunkLen
        } while offset < bytes.count
    }

    private func writeFragment(_ frame: Data, to characteristic: CBCharacteristic, type: CBCharacteristicWriteType) async throws {
        guard let peripheral = connectedPeripheral else { throw BluetoothError.notConnected }
        if type == .withoutResponse {
            peripheral.writeValue(frame, for: characteristic, type: .withoutResponse)
            return
        }
        let result: Result<Void, Error> = await withCheckedContinuation { cont in
            writeCompletions[characteristic.uuid] = { error in
                if let error {
                    cont.resume(returning: .failure(error))
                } else {
                    cont.resume(returning: .success(()))
                }
            }
            peripheral.writeValue(frame, for: characteristic, type: .withResponse)
        }
        try result.get()
    }

    /// Subscribe to characteristic notifications. The handler is called on the main actor for every update.
    /// If called before the peripheral is connected, the subscription is applied automatically once characteristics are discovered.
    func subscribe(to char: CharacteristicUUID, handler: @escaping @MainActor (Data) -> Void) {
        notificationHandlers[char.uuid] = handler
        guard let characteristic = discoveredCharacteristics[char.uuid],
              let peripheral = connectedPeripheral,
              !characteristic.isNotifying else {
            print("Could not subscribe to \(char.uuid) — characteristic not found or already notifying.")
            return
        }
        peripheral.setNotifyValue(true, for: characteristic)
        print("Subscribed to \(char.uuid)")
    }

    func unsubscribe(from char: CharacteristicUUID) {
        notificationHandlers.removeValue(forKey: char.uuid)
        guard let characteristic = discoveredCharacteristics[char.uuid],
              let peripheral = connectedPeripheral,
              characteristic.isNotifying else { return }
        peripheral.setNotifyValue(false, for: characteristic)
    }

    /// Enables notifications for a characteristic with no persistent handler — used for
    /// characteristics only ever pulled on demand via `awaitNextUpdate(_:)`, which still
    /// need their CCCD written so the base station's notifications reach us at all.
    func enableNotifications(for char: CharacteristicUUID) {
        guard let characteristic = discoveredCharacteristics[char.uuid],
              let peripheral = connectedPeripheral,
              !characteristic.isNotifying else { return }
        peripheral.setNotifyValue(true, for: characteristic)
    }

    func clearSubscriptions() {
        notificationHandlers.removeAll()
        pendingRawReads.removeAll()
        reassemblyBuffers.removeAll()
    }

    // MARK: - CBCentralManagerDelegate

    /// Called on the main actor when the Bluetooth state changes. Updates the `bluetoothState` property and starts scanning if necessary.
    public nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Task { @MainActor [weak self] in
            guard let self else { return }
            self.bluetoothState = central.state
            switch central.state {
            case .poweredOn:
                if self.pendingStartScan {
                    self.pendingStartScan = false
                    self.beginScan()
                }
            default:
                self.isScanning = false
                self.discoveredBaseStations = []
            }
        }
    }

    /// Called on the main actor when a peripheral is discovered. Adds it to the `discoveredBaseStations` array if not
    /// already present, or fills in its build date if a later packet (e.g. the scan response) carries one we didn't
    /// have yet. With `CBCentralManagerScanOptionAllowDuplicatesKey: true`, the primary advertisement and scan
    /// response typically arrive as separate `didDiscover` calls rather than merged into one, so the manufacturer
    /// data (build date) is not guaranteed to be present on the very first call for a peripheral.
    public nonisolated func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let buildDate = parseBaseStationBuildDate(fromAdvertisementData: advertisementData)
        Task { @MainActor [weak self] in
            guard let self else { return }
            if let index = self.discoveredBaseStations.firstIndex(where: { $0.peripheral.identifier == peripheral.identifier }) {
                if let buildDate, self.discoveredBaseStations[index].buildDate == nil {
                    self.discoveredBaseStations[index] = DiscoveredBaseStation(peripheral: peripheral, buildDate: buildDate)
                }
                return
            }
            self.discoveredBaseStations.append(DiscoveredBaseStation(peripheral: peripheral, buildDate: buildDate))
        }
    }

    /// Called on the main actor when a peripheral is connected. Sets the `connectedPeripheral` property, assigns the delegate, and starts service discovery.
    public nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        Task { @MainActor [weak self] in
            guard let self else { return }
            self.connectedPeripheral = peripheral
            peripheral.delegate = self
            peripheral.discoverServices([ServiceUUID])
        }
    }

    /// Called on the main actor when a peripheral is disconnected. Clears the `connectedPeripheral` property and resets the connection state. If `shouldReconnect` is true, attempts to reconnect to the peripheral.
    public nonisolated func centralManager(
        _ central: CBCentralManager,
        didDisconnectPeripheral peripheral: CBPeripheral,
        error: Error?
    ) {
        Task { @MainActor [weak self] in
            self?.handleDisconnect(from: peripheral)
        }
    }

    /// Called on the main actor when a connection attempt fails
    public nonisolated func centralManager(
        _ central: CBCentralManager,
        didFailToConnect peripheral: CBPeripheral,
        error: Error?
    ) {
        Task { @MainActor [weak self] in
            self?.state.connectionStateBaseStation = .DISCONNECTED
        }
    }

    // MARK: - CBPeripheralDelegate

    /// Called on the main actor when services are discovered. If the expected service is found, starts characteristic discovery for that service.
    public nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        Task { @MainActor in
            guard error == nil,
                  let service = peripheral.services?.first(where: { $0.uuid == ServiceUUID }) else { return }
            peripheral.discoverCharacteristics(CharacteristicUUID.allCases.map(\.uuid), for: service)
        }
    }

    /// Called on the main actor when characteristics are discovered for a service. Updates the `discoveredCharacteristics` dictionary and applies any pending subscriptions.
    public nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        Task { @MainActor [weak self] in
            guard let self, error == nil else { return }
            for char in service.characteristics ?? [] {
                self.discoveredCharacteristics[char.uuid] = char
            }
            self.state.connectionStateBaseStation = .CONNECTED
            // Apply any subscriptions that were registered before connection
            for (uuid, _) in self.notificationHandlers {
                if let char = self.discoveredCharacteristics[uuid], !char.isNotifying {
                    peripheral.setNotifyValue(true, for: char)
                }
            }
            afterFullyConnected(service: service)
        }
    }

    public nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        Task { @MainActor [weak self] in
            guard let self else { return }
            let uuid = characteristic.uuid

            guard error == nil, let raw = characteristic.value else {
                self.pendingRawReads.remove(uuid)
                if let completion = self.readCompletions.removeValue(forKey: uuid) {
                    completion(nil)
                }
                return
            }

            // A plain GATT read response is delivered through this same delegate method,
            // unframed — route it straight to the pending continuation without treating
            // it as a notification fragment (see readCharacteristic/pendingRawReads).
            if self.pendingRawReads.remove(uuid) != nil {
                if let completion = self.readCompletions.removeValue(forKey: uuid) {
                    completion(raw)
                }
                return
            }

            var buffer = self.reassemblyBuffers[uuid] ?? FragmentReassemblyBuffer()
            let complete = buffer.ingest(raw)
            self.reassemblyBuffers[uuid] = buffer
            guard let complete else { return }

            print("Received notification from \(uuid)")
            // Satisfy a pending awaitNextUpdate(_:), if any.
            if let completion = self.readCompletions.removeValue(forKey: uuid) {
                completion(complete)
            }
            // Forward to notification subscriber
            if let handler = self.notificationHandlers[uuid] {
                handler(complete)
            }
        }
    }

    public nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didWriteValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        Task { @MainActor [weak self] in
            guard let self else { return }
            if let completion = self.writeCompletions.removeValue(forKey: characteristic.uuid) {
                completion(error)
            }
        }
    }

    // MARK: - Private

    private func beginScan() {
        discoveredBaseStations = []
        isScanning = true
        centralManager?
            .scanForPeripherals(withServices: [ServiceUUID], options: [
                CBCentralManagerScanOptionAllowDuplicatesKey: true,
            ])
    }
}
