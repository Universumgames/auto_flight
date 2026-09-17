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
    /// Per-characteristic fragment reassembly state for notified values.
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
        for topic in CharacteristicUUID.allCases {
            subscribe(to: topic, handler: { data in self.onNotify(topic, data) })
        }
    }
    
    func onNotify(_ topic: CharacteristicUUID, _ data: Data){
        switch(topic){
            case .allUpdate:
                print("Unexcpected packet")
            case .positionUpdate:
                onFramePositionData(data: data)
            case .connectionUpdate:
                onFrameConnectionUpdate(data: data)
            case .sensorData:
                onFrameSensorUpdate(data: data)
            case .areaDefine:
                // TODO: onAreaDefine Receive
                break
            case .plannedRoute:
                onFrameRoutePlanned(data: data)
            case .batteryStatus:
                onFrameBatteryStatus(data: data)
        }
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

    private func onFramePositionData(data: Data) {
        guard let packet = wire.FrontendPackets.shared.decodePositionUpdate(hex: data.hexEncoded) else {
            return
        }
        PacketParsing.applyPositionPacket(packet)
        print("Flight update received: \(packet)")
    }

    private func onFrameBatteryStatus(data: Data) {
        guard let packet = wire.FrontendPackets.shared.decodeBatteryStatus(hex: data.hexEncoded) else { return }
        PacketParsing.applyBatteryStatusPacket(state, packet)
        print("Battery status received: \(packet)")
    }
    
    private func onFrameRoutePlanned(data: Data){
        guard let packet = wire.FrontendPackets.shared.decodePlannedRoute(
            hex: data.hexEncoded
        ) else { return }
        PacketParsing.applyRoutePlannedPacket(state, packet)
        print("Route plan update received: \(packet)")
    }

    // MARK: - Manual fetching

    /// Waits for the next already-subscribed notification on `char` — every topic is
    /// only ever served over the fragmented notify path now (see BluetoothManager.swift's
    /// `didUpdateValueFor`), so this is the one way to pull a value on demand.
    func awaitNextUpdate(_ char: CharacteristicUUID) async -> Data? {
        guard discoveredCharacteristics[char.uuid] != nil else { return nil }
        return await withCheckedContinuation { continuation in
            readCompletions[char.uuid] = { continuation.resume(returning: $0) }
        }
    }

    func submitArea(polygon: [Coordinate], onResult: @escaping (AreaSubmitResult) -> Void) {
        Task {
            guard let data = Data(hexEncoded: wire.FrontendPackets.shared.encode(packet: wire.AreaDefinePacket(sourceId: 0, shape: polygon))) else {
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
}
