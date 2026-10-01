//
//  PlaneInfo.swift
//  iosApp
//
//  Created by Tom Arlt on 23.08.26.
//

import Observation
import SharedLogic
import Foundation

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
    /// The area polygon submitted in `AreaView`, kept around so `RouteView` can
    /// center its map on it while waiting for the base station's planned route.
    var area: AreaData?

    var wizardStep: [ConfigurationState] = []

    init(id: PlaneID) {
        self.id = id
    }

    var connectionItem: ConnectionItem {
        let gpsLabel = position.map {
            position in String(localized: "connection.items.subtask.gpsPositionWithCoords \(String(format: "%.6f", position.latitude)), \(String(format: "%.6f", position.longitude))")
        } ?? String(localized: "connection.items.subtask.gpsPosition")
        let autopilotLabel = manualOverride
            ? String(localized: "connection.items.subtask.autopilotControlManualOverride")
            : String(localized: "connection.items.subtask.autopilotControl")
        let magnetometerLabel = magnetometerConnection == .CONNECTED
            ? String(localized: "connection.items.subtask.magnetometerWithHeading \(String(format: "%.2f", heading))")
            : String(localized: "connection.items.subtask.magnetometer")
        let barometerLabel = pressure != 0
            ? String(localized: "connection.items.subtask.barometerWithPressure \(String(format: "%.2f", pressure))")
            : String(localized: "connection.items.subtask.barometer")

        return ConnectionItem(
            label: String(
                localized: "connection.items.plane.label \(String(format: "%X", id))"
            ),
            connectionItemType: .PLANE,
            status: connectionState,
            batteryPercent: Int32(batteryPercentage),
            subTasks: [
                SubTask(label: String(localized: "connection.items.subtask.connection"), state: stateOf(connectionState == .CONNECTED)),
                SubTask(label: gpsLabel, state: stateOf(gpsConnection == .CONNECTED)),
                SubTask(label: String(localized: "connection.items.subtask.motorController"), state: stateOf(motorComConnection == .CONNECTED)),
                SubTask(
                    label: autopilotLabel,
                    state: stateOf(
                        !manualOverride && motorComConnection == .CONNECTED
                    )
                ),
                SubTask(label: magnetometerLabel, state: stateOf(magnetometerConnection == .CONNECTED)),
                SubTask(label: String(localized: "connection.items.subtask.accelerometer"), state: stateOf(accelerometerConnection == .CONNECTED)),
                SubTask(label: barometerLabel, state: stateOf(barometerConnection == .CONNECTED)),
            ]
        )
    }

    static var defaultPlaneID: PlaneID { SharedLogic.DEFAULT_PLANE_ID }
    static var defaultPlane: PlaneInfo {
        PlaneInfo(id: defaultPlaneID)
    }
}

@Observable
final class AreaData {
    var areaPoints: [Coordinate]
    var settings: wire.RouteSettings
    
    init(areaPoints: [Coordinate], settings: wire.RouteSettings) {
        self.areaPoints = areaPoints
        self.settings = settings
    }
}
