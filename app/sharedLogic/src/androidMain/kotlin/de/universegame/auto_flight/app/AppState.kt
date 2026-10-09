package de.universegame.auto_flight.app

import de.universegame.auto_flight.app.wire.ConnectionState
import de.universegame.auto_flight.app.wire.FlightState

/**
 * Snapshot of everything a single connected plane reports: connection health,
 * position, route and sensor readings. Held in [AppState.planes], keyed by
 * [PlaneID], so the app can track more than one plane at once.
 *
 * A `data class` of `var`s rather than an immutable value type, so an existing
 * entry in [AppState.planes] can be mutated in place as packets arrive instead of
 * being replaced on every update.
 */
data class PlaneInfo(
    val id: PlaneID,
    var connectionState: ConnectionState = ConnectionState.CONNECTING,
    var position: Coordinate? = null,
    var positionUpdateTime: Long? = null,
    var lastContactTimestamp: Long? = null,
    var gpsConnection: ConnectionState = ConnectionState.CONNECTING,
    var barometerConnection: ConnectionState = ConnectionState.CONNECTING,
    var motorComConnection: ConnectionState = ConnectionState.CONNECTING,
    var magnetometerConnection: ConnectionState = ConnectionState.CONNECTING,
    var accelerometerConnection: ConnectionState = ConnectionState.CONNECTING,
    var manualOverride: Boolean = false,
    var flightState: FlightState = FlightState.PLANNING,
    var pressure: Double = 0.0,
    var calculatedAltitude: Double = 0.0,
    var heading: Double = 0.0,
    var flightRoute: List<Coordinate>? = null,
    var flightRouteUpdateTime: Long? = null,
    var plannedRoute: List<Coordinate>? = null,
    var plannedRouteUpdateTime: Long? = null,
)

/**
 * Mutable snapshot of everything the UI needs to render: base-station connection
 * health/position plus the connected planes. Produced by [PacketParser] and handed
 * out by [FlightRepository] implementations.
 *
 * Declared as a class of `var`s (rather than an immutable data class) so it can be
 * mutated in place as packets arrive, instead of rebuilding a full snapshot on every
 * update. Android/JVM-only (see this file's package-level placement note in
 * `Models.kt`) - the iOS app has its own native `AppState` (`Models/AppState.swift`).
 */
class AppState(
    var connectionStateBaseStation: ConnectionState = ConnectionState.CONNECTING,
    var basePosition: Coordinate? = null,
    var basePositionUpdateTime: Long? = null,
    var lastContactBaseStationTimestamp: Long? = null,
    var gpsConnectionBase: ConnectionState = ConnectionState.CONNECTING,
    var barometerConnectionBase: ConnectionState = ConnectionState.CONNECTING,
    var pressureBase: Double = 0.0,
) {
    val planes: MutableMap<PlaneID, PlaneInfo> = mutableMapOf()
}
