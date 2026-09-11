import CoreBluetooth
import Foundation
@preconcurrency import SharedLogic
import SwiftUI

/// Native Swift counterpart to `sharedLogic`'s (Android-only) `KtorFlightRepository`.
/// Talks to the base station with `URLSession` directly - see the module-level
/// comment in `sharedLogic/build.gradle.kts` for why the Ktor implementation can't
/// be shared with iOS. Mirrors the same websocket + heartbeat + REST behavior.
@MainActor
@Observable
final class ConnectionManager: NSObject {
    let state = AppState.shared

    var host: String {
        get {
            access(keyPath: \.host)
            return UserDefaults.standard.string(forKey: "host") ?? ""
        }
        set {
            withMutation(keyPath: \.host) {
                UserDefaults.standard.setValue(newValue, forKey: "host")
            }
        }
    }

    private var shouldReconnect = false
    private var reconnectTask: Task<Void, Never>?
    internal var centralManager: CBCentralManager?

    // Bluetooth scanning state — updated by BluetoothManager extension
    var bluetoothState: CBManagerState = .unknown
    var discoveredBaseStations: [DiscoveredBaseStation] = []
    var isScanning = false
    internal var pendingStartScan = false

    // Bluetooth connection state — updated by BluetoothManager extension
    var connectedPeripheral: CBPeripheral?
    internal var discoveredCharacteristics: [CBUUID: CBCharacteristic] = [:]
    internal var readCompletions: [CBUUID: (Data?) -> Void] = [:]
    internal var writeCompletions: [CBUUID: (Error?) -> Void] = [:]
    internal var notificationHandlers: [CBUUID: (Data) -> Void] = [:]
    /// UUIDs of characteristics whose next `didUpdateValueFor` delivery is a plain,
    /// unfragmented GATT read response rather than a fragmented notification.
    internal var pendingRawReads: Set<CBUUID> = []
    /// Per-characteristic fragment reassembly state for notified (not read) values.
    internal var reassemblyBuffers: [CBUUID: FragmentReassemblyBuffer] = [:]

    override init() {
        super.init()
    }

    func tryConnectToPeripheral(_ peripheral: CBPeripheral) {
        stopBluetoothScan()
        shouldReconnect = true
        state.connectionStateBaseStation = .CONNECTING
        centralManager?.connect(peripheral, options: nil)
    }

    func afterFullyConnected(service: CBService) {
        subscribe(to: CharacteristicUUID.connectionUpdate, handler: onFrameConnectionUpdate)
        subscribe(to: CharacteristicUUID.sensorData, handler: onFrameSensorUpdate)
        subscribe(to: CharacteristicUUID.batteryStatus, handler: onFrameBatteryStatus)
        subscribe(to: CharacteristicUUID.flightUpdate, handler: onFrameFlightData)
        // areaDefine and plannedRoute have no persistent handler — they're only ever
        // pulled on demand via awaitNextUpdate(_:) below — but still need their CCCD
        // enabled so the base station's notifications for them reach us at all.
        enableNotifications(for: .areaDefine)
        enableNotifications(for: .plannedRoute)
    }

    func disconnect() {
        shouldReconnect = false
        reconnectTask?.cancel()
        if let peripheral = connectedPeripheral {
            centralManager?.cancelPeripheralConnection(peripheral)
        }
        clearSubscriptions()
        state.connectionStateBaseStation = .DISCONNECTED
    }

    func handleDisconnect(from peripheral: CBPeripheral) {
        connectedPeripheral = nil
        discoveredCharacteristics = [:]
        state.connectionStateBaseStation = .DISCONNECTED
        state.planes.values.forEach { $0.connectionState = .CONNECTING }

        guard shouldReconnect else { return }
        // CoreBluetooth retries in the background until cancelPeripheralConnection is called
        clearSubscriptions()
        centralManager?.connect(peripheral, options: nil)
    }
    
    // MARK: - Notifications

    private func onFrameConnectionUpdate(data: Data) {
        guard let packet = wire.FrontendPackets.shared.decodeConnectionUpdate(hex: data.hexEncoded) else { return }
        PacketParsing.applyConnectionPacket(state, packet)
        print("Connection update received: \(packet)")
    }

    private func onFrameSensorUpdate(data: Data) {
        guard let packet = wire.FrontendPackets.shared.decodeSensor(hex: data.hexEncoded) else { return }
        PacketParsing.applySensorPacket(state, packet)
        print("Sensor update received: \(packet)")
    }

    private func onFrameFlightData(data: Data) {
        guard let packet = wire.FrontendPackets.shared.decodeFlightUpdate(hex: data.hexEncoded) else { return }
        PacketParsing.applyFlightPacket(state, packet)
        print("Flight update received: \(packet)")
    }

    private func onFrameBatteryStatus(data: Data) {
        guard let packet = wire.FrontendPackets.shared.decodeBatteryStatus(hex: data.hexEncoded) else { return }
        PacketParsing.applyBatteryStatusPacket(state, packet)
        print("Battery status received: \(packet)")
    }

    // MARK: - Manual fetching

    /// Waits for the next already-subscribed notification on `char`. Unlike
    /// `readCharacteristic`, this issues no GATT read — it's for characteristics
    /// (areaDefine, flightUpdate, plannedRoute) whose payload can exceed the 512-byte
    /// GATT attribute limit and so are only ever served over the fragmented notify path.
    func awaitNextUpdate(_ char: CharacteristicUUID) async -> Data? {
        guard discoveredCharacteristics[char.uuid] != nil else { return nil }
        return await withCheckedContinuation { continuation in
            readCompletions[char.uuid] = { continuation.resume(returning: $0) }
        }
    }

    func fetchArea(onResult: @escaping ([Coordinate]?) -> Void) {
        Task {
            let data = await awaitNextUpdate(.areaDefine)
            if let data, let packet = wire.FrontendPackets.shared.decodeAreaDefine(hex: data.hexEncoded) {
                Task { @MainActor in onResult(packet.shape) }
                return
            } else {
                onResult(nil)
            }
        }
    }

    func submitArea(polygon: [Coordinate], onResult: @escaping (AreaSubmitResult) -> Void) {
        Task {
            guard let data = Data(hexEncoded: wire.FrontendPackets.shared.encode(packet: wire.AreaDefinePacket(type: "area", shape: polygon))) else {
                onResult(.FAILED); return
            }
            do {
                try await writeCharacteristic(.areaDefine, data: data)
                Task { @MainActor in onResult(.ACCEPTED) }
            } catch {
                onResult(.FAILED)
            }
        }
    }

    func fetchRoute(onResult: @escaping ([Coordinate]) -> Void) {
        Task {
            let data = await awaitNextUpdate(.plannedRoute)
            if let data, let packet = wire.FrontendPackets.shared.decodePlannedRoute(hex: data.hexEncoded) {
                Task { @MainActor in onResult(packet.route) }
            } else {
                onResult([])
            }
        }
    }

    func fetchFlightState(onResult: @escaping (wire.FlightUpdatePacket?) -> Void) {
        Task {
            let data = await awaitNextUpdate(.flightUpdate)
            if let data, let packet = wire.FrontendPackets.shared.decodeFlightUpdate(hex: data.hexEncoded) {
                Task { @MainActor in onResult(packet) }
            } else {
                onResult(nil)
            }
        }
    }
    
    func fetchConnectionState(onResult: @escaping (wire.ConnectionUpdatePacket?) -> Void) {
        Task {
            let data = await readCharacteristic(.connectionUpdate)
            if let data, let packet = wire.FrontendPackets.shared.decodeConnectionUpdate(hex: data.hexEncoded) {
                Task { @MainActor in onResult(packet) }
            } else {
                onResult(nil)
            }
        }
    }
    
    func fetchSensorState(onResult: @escaping (wire.SensorPacket?) -> Void) {
        Task {
            let data = await readCharacteristic(.sensorData)
            if let data, let packet = wire.FrontendPackets.shared.decodeSensor(hex: data.hexEncoded) {
                Task { @MainActor in onResult(packet) }
            } else {
                onResult(nil)
            }
        }
    }

    func fetchBatteryStatus(onResult: @escaping (wire.BatteryStatusPacket?) -> Void) {
        Task {
            let data = await readCharacteristic(.batteryStatus)
            if let data, let packet = wire.FrontendPackets.shared.decodeBatteryStatus(hex: data.hexEncoded) {
                Task { @MainActor in onResult(packet) }
            } else {
                onResult(nil)
            }
        }
    }
}
