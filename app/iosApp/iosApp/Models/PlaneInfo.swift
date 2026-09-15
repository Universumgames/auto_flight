//
//  PlaneInfo.swift
//  iosApp
//
//  Created by Tom Arlt on 23.08.26.
//


import Observation
import SharedLogic

/// Native Swift counterpart to the shared Kotlin `PlaneInfo` (`sharedLogic/.../Models.kt`):
/// everything a single connected plane reports. Held in `AppState.planes`, keyed by
/// `PlaneID`, mirroring the Kotlin side's `AppState.planes: MutableMap<PlaneID, PlaneInfo>`.
///
/// Being `@Observable` lets SwiftUI views that read individual fields
/// (`plane.position`, ...) track and re-render on just those fields, the same way
/// `AppState` itself is `@Observable` - see that type's header comment for why this
/// can't literally share a type with the Kotlin side.
@Observable
final class PlaneInfo {
    let id: PlaneID
    var connectionState: ConnectionState = .CONNECTING
    var position: Coordinate?
    var positionUpdateTime: Int64?
    var lastContactTimestamp: Int64?
    var gpsConnection: ConnectionState = .CONNECTING
    var barometerConnection: ConnectionState = .CONNECTING
    var motorComConnection: ConnectionState = .CONNECTING
    var magnetometerConnection: ConnectionState = .CONNECTING
    var accelerometerConnection: ConnectionState = .CONNECTING
    var manualOverride: Bool = false
    var flightState: FlightState = .PLANNING
    var pressure: Double = 0
    var batteryPercentage: Int = -1
    var calculatedAltitude: Double = 0
    var heading: Double = 0
    var flightRoute: [Coordinate]?
    var flightRouteUpdateTime: Int64?
    var plannedRoute: [Coordinate]?
    var plannedRouteUpdateTime: Int64?

    init(id: PlaneID) {
        self.id = id
    }
    
    var connectionItem: ConnectionItem {
        let gpsLabel = position.map {
            _ in String(localized: "connection.items.subtask.gpsPositionWithCoords")
        } ?? String(localized: "connection.items.subtask.gpsPosition")
        let autopilotLabel = manualOverride
            ? String(localized: "connection.items.subtask.autopilotControlManualOverride")
            : String(localized: "connection.items.subtask.autopilotControl")
        let magnetometerLabel = magnetometerConnection == .CONNECTED
            ? String(localized: "connection.items.subtask.magnetometerWithHeading")
            : String(localized: "connection.items.subtask.magnetometer")
        let barometerLabel = pressure != 0
            ? String(localized: "connection.items.subtask.barometerWithPressure")
            : String(localized: "connection.items.subtask.barometer")

        return ConnectionItem(
            label: String(localized: "connection.items.plane.label"),
            connectionItemType: .PLANE,
            status: connectionState,
            batteryPercent: Int32(batteryPercentage),
            subTasks: [
                SubTask(label: String(localized: "connection.items.subtask.connection"), state: stateOf(connectionState == .CONNECTED)),
                SubTask(label: gpsLabel, state: stateOf(gpsConnection == .CONNECTED)),
                SubTask(label: String(localized: "connection.items.subtask.motorController"), state: stateOf(motorComConnection == .CONNECTED)),
                SubTask(label: autopilotLabel, state: stateOf(!manualOverride)),
                SubTask(label: magnetometerLabel, state: stateOf(magnetometerConnection == .CONNECTED)),
                SubTask(label: String(localized: "connection.items.subtask.accelerometer"), state: stateOf(accelerometerConnection == .CONNECTED)),
                SubTask(label: barometerLabel, state: stateOf(barometerConnection == .CONNECTED)),
            ]
        )
    }
    
    static var defaultPlaneID: PlaneID { "default" }
    static var defaultPlane: PlaneInfo {
        PlaneInfo(id: defaultPlaneID)
    }
}
