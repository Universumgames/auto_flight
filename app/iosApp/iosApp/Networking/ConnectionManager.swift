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

    internal struct WaitRequestKey: Hashable {
        let characteristic: CharacteristicUUID
        let sourceId: PlaneID
    }

    internal var waitingRequests: [WaitRequestKey: [(Data?) -> Void]] = [:]

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

        CharacteristicUUID.allCases.forEach { uuid in
            Task {
                await requestUpdate(uuid)
            }
        }
    }

    func onNotify(_ topic: CharacteristicUUID, _ data: Data) {
        switch topic {
        case .position:
            onFramePositionData(data: data)
        case .componentStatus:
            onFrameConnectionUpdate(data: data)
        case .sensorUpdate:
            onFrameSensorUpdate(data: data)
        case .plannedArea:
            // TODO: onAreaDefine Receive
            break
        case .plannedRoute:
            onFrameRoutePlanned(data: data)
        case .routeHistory:
            print("route history unhandles")
        case .routeHistoryRequest:
            print("route request unhandled")
        case .plannedRouteConfirmation:
            print("route confirm unhandled")
        }
        guard let sourceId = wire.FrontendPackets.shared.decodeBase(
            hex: data.hexEncoded
        )?.id else { return }
        print("Received notification for topic \(topic) from source \(sourceId)")
        let key = WaitRequestKey(characteristic: topic, sourceId: sourceId)
        let callbacks = waitingRequests[key]
        waitingRequests[key] = []
        callbacks?.forEach { callback in
            callback(data)
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
        guard let packet = wire.FrontendPackets.shared.decodeComponentStatus(hex: data.hexEncoded) else {
            return
        }
        PacketParsing.applyConnectionPacket(state, packet)
        print("Connection update received: \(packet)")
    }

    private func onFrameSensorUpdate(data: Data) {
        guard let packet = wire.FrontendPackets.shared.decodeSensorUpdate(hex: data.hexEncoded) else {
            return
        }
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

    private func onFrameRoutePlanned(data: Data) {
        guard let packet = wire.FrontendPackets.shared.decodePlannedRoute(
            hex: data.hexEncoded
        ) else { return }
        PacketParsing.applyRoutePlannedPacket(state, packet)
        print("Route plan update received: \(packet)")
    }

    private func onFrameAreaDefine(data: Data) {
        guard let packet = wire.FrontendPackets.shared.decodePlannedArea(
            hex: data.hexEncoded
        ) else { return }
        PacketParsing.applyAreaDefinePacket(state, packet)
        print("Area define update received: \(packet)")
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

    func submitArea(planeId: PlaneID, polygon: [Coordinate], routeSettings: wire.RouteSettings, onResult: @escaping (AreaSubmitResult) -> Void) {
        Task {
            guard let data = Data(hexEncoded: wire.FrontendPackets.shared.encode(packet: wire.PlannedAreaPacket(id: planeId, shape: polygon, settings: routeSettings, timestamp: 0))) else {
                onResult(.FAILED); return
            }
            do {
                try await writeCharacteristic(.plannedArea, data: data)
                Task { @MainActor in onResult(.ACCEPTED) }
            } catch {
                onResult(.FAILED)
            }
        }
    }

    func queryPositions() {
        Task {
            await requestUpdate(CharacteristicUUID.position)
        }
    }

    func queryConnections() {
        Task {
            await requestUpdate(CharacteristicUUID.componentStatus)
        }
    }

    func querySensorData() {
        Task {
            await requestUpdate(CharacteristicUUID.sensorUpdate)
        }
    }

    func queryAreas() {
        Task {
            await requestUpdate(CharacteristicUUID.plannedArea)
        }
    }

    func queryRoutes() {
        Task {
            await requestUpdate(CharacteristicUUID.plannedRoute)
        }
    }

    private func querySingle(_ sourceId: PlaneID, _ characteristic: CharacteristicUUID) async -> Data? {
        let key = WaitRequestKey(
            characteristic: characteristic,
            sourceId: sourceId
        )
        return await withCheckedContinuation { continuation in
            waitingRequests[key, default: []].append { data in
                continuation.resume(returning: data)
            }
        }
    }

    func queryPosition(_ sourceId: PlaneID) async -> Coordinate? {
        guard let data = await querySingle(
            sourceId,
            CharacteristicUUID.position
        ) else { return nil }
        return wire.FrontendPackets.shared
            .decodePositionUpdate(hex: data.hexEncoded)?.position
    }

    func queryConnection(_ sourceId: PlaneID) async -> wire.ComponentStatus? {
        guard let data = await querySingle(
            sourceId,
            CharacteristicUUID.componentStatus
        ) else { return nil }
        return wire.FrontendPackets.shared
            .decodeComponentStatus(hex: data.hexEncoded)
    }

    func queryArea(_ sourceId: PlaneID) async -> wire.PlannedAreaPacket? {
        guard let data = await querySingle(
            sourceId,
            CharacteristicUUID.plannedArea
        ) else { return nil }
        return wire.FrontendPackets.shared
            .decodePlannedArea(hex: data.hexEncoded)
    }

    func queryRoute(_ sourceId: PlaneID) async -> [Coordinate]? {
        guard let data = await querySingle(
            sourceId,
            CharacteristicUUID.plannedRoute
        ) else { return nil }
        return wire.FrontendPackets.shared
            .decodePlannedRoute(hex: data.hexEncoded)?.route
    }
}
