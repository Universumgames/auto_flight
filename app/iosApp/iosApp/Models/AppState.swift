import Observation
import SharedLogic

/// Native Swift counterpart to the shared Kotlin `AppState` (`sharedLogic/.../Models.kt`).
/// Kotlin's Swift Export (still experimental) requires interface conformers to inherit
/// `KotlinBase` - i.e. only Kotlin-originated objects can implement an exported
/// interface - so this can't literally share a type with the Kotlin side. It's a
/// field-for-field mirror declared natively instead, the same way `PacketParsing.swift`
/// reimplements `PacketParser.kt` (see that file's header comment). The extension below
/// similarly reimplements `ConnectionItems.kt`'s derivation functions as getters, since
/// those take the shared Kotlin `AppState` interface as a parameter and hit the same
/// `KotlinBase` restriction.
///
/// Being `@Observable` lets SwiftUI views that read individual fields
/// (`store.state.basePosition`, ...) track and re-render on just those fields, without
/// any manual `@Published`/`ObservableObject` plumbing and without rebuilding a whole
/// new snapshot on every packet - fields are mutated directly by `PacketParsing` and
/// `ConnectionManager`.
@Observable
final class AppState {
    public static let shared = AppState()

    var navigationPath: [ConfigurationState] = []
    var wizardStep: ConfigurationState { navigationPath.last ?? .CONNECTION }

    func resetNavigation() { navigationPath = [] }

    var connectionStateBaseStation: ConnectionState = .DISCONNECTED
    var basePosition: Coordinate?
    var basePositionUpdateTime: Int64?
    var lastContactBaseStationTimestamp: Int64?
    var gpsConnectionBase: ConnectionState = .CONNECTING
    var barometerConnectionBase: ConnectionState = .CONNECTING
    var pressureBase: Double = 0
    var planes: [PlaneID: PlaneInfo] = [:]
}

/// Derives the connection-overview presentation (grouped links + sub-tasks + the
/// global warning flags shown in the app chrome). The value types returned -
/// `ConnectionItem`, `SubTask`, `SubTaskState` - are plain Kotlin data classes/enums,
/// which export to Swift without the `KotlinBase` restriction, so those are reused
/// as-is; likewise `ConnectionItems.shared.isConnected`/`.formatStatus`, which only
/// take `ConnectionState`.
extension AppState {
    var baseStationItem: ConnectionItem {
        ConnectionItem(
            label: "Base Station",
            status: connectionStateBaseStation,
            subTasks: [
                SubTask(label: "Connection", state: stateOf(connectionStateBaseStation == .CONNECTED)),
                SubTask(
                    label: "GPS Position" + (basePosition.map {
                        " (\(formatCoordinate($0.latitude)), \(formatCoordinate($0.longitude)))"
                    } ?? ""),
                    state: stateOf(gpsConnectionBase == .CONNECTED)
                ),
                SubTask(
                    label: "Barometer" + (pressureBase != 0 ? " (\(formatPressure(pressureBase))hPa)" : ""),
                    state: stateOf(barometerConnectionBase == .CONNECTED)
                ),
            ]
        )
    }

    /// One `ConnectionItem` per connected plane, keyed by `planes`.
    var planeItems: [ConnectionItem] {
        let planes = planes.isEmpty ? [PlaneInfo(id: defaultPlaneID)] : Array(planes.values)
        return planes.map { $0.connectionItem }
    }

    var totalIsConnected: Bool {
        baseStationItem.status == .CONNECTED && planeItems.allSatisfy { $0.status == .CONNECTED }
    }

    /// True when a plane and its motor controller are connected but autopilot control is disabled.
    var autopilotDisabledWarning: Bool {
        planes.values.contains { $0.connectionState == .CONNECTED && $0.motorComConnection == .CONNECTED && $0.manualOverride }
    }

    /// True when a plane is connected but its motor controller is not.
    var motorControllerDisconnectedError: Bool {
        planes.values.contains { $0.connectionState == .CONNECTED && $0.motorComConnection != .CONNECTED }
    }

    /// True when a plane is connected but no GPS position is available for it.
    var gpsPlaneUnavailableError: Bool {
        planes.values.contains { $0.connectionState == .CONNECTED && $0.gpsConnection != .CONNECTED }
    }
}

func stateOf(_ done: Bool) -> SubTaskState {
    done ? .DONE : .LOADING
}

func formatCoordinate(_ value: Double) -> String {
    String((value * 1_000_000).rounded() / 1_000_000)
}

func formatPressure(_ value: Double) -> String {
    String((value * 100).rounded() / 100)
}
