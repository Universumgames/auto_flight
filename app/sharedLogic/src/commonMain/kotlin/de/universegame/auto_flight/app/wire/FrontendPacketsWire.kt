package de.universegame.auto_flight.app.wire

import de.universegame.auto_flight.app.Coordinate
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

/**
 * `internal` (module-scoped) `@Serializable` twins of the public wire types in
 * FrontendPackets.kt, plus the conversions to/from them. Kept out of that file's public API
 * surface - see the doc comment on [FrontendPackets] there for why: Swift Export only ever
 * bridges `public` declarations, so `internal` is enough to keep kotlinx-serialization-core out
 * of its view, same as `private` would, without forcing everything into one file.
 */

@Serializable
internal data class CoordinateWire(val latitude: Double, val longitude: Double)

internal fun Coordinate.toWire() = CoordinateWire(latitude, longitude)
internal fun CoordinateWire.toPublic() = Coordinate(latitude, longitude)

@Serializable
internal enum class ConnectionStateWire {
    @SerialName("connecting") CONNECTING,
    @SerialName("connected") CONNECTED,
}

internal fun ConnectionState.toWire() = when (this) {
    ConnectionState.CONNECTING -> ConnectionStateWire.CONNECTING
    ConnectionState.CONNECTED -> ConnectionStateWire.CONNECTED
}

internal fun ConnectionStateWire.toPublic() = when (this) {
    ConnectionStateWire.CONNECTING -> ConnectionState.CONNECTING
    ConnectionStateWire.CONNECTED -> ConnectionState.CONNECTED
}

@Serializable
internal enum class FlightStateWire {
    @SerialName("planning") PLANNING,
    @SerialName("planned") PLANNED,
    @SerialName("flying") FLYING,
    @SerialName("returning") RETURNING,
}

internal fun FlightState.toWire() = when (this) {
    FlightState.PLANNING -> FlightStateWire.PLANNING
    FlightState.PLANNED -> FlightStateWire.PLANNED
    FlightState.FLYING -> FlightStateWire.FLYING
    FlightState.RETURNING -> FlightStateWire.RETURNING
}

internal fun FlightStateWire.toPublic() = when (this) {
    FlightStateWire.PLANNING -> FlightState.PLANNING
    FlightStateWire.PLANNED -> FlightState.PLANNED
    FlightStateWire.FLYING -> FlightState.FLYING
    FlightStateWire.RETURNING -> FlightState.RETURNING
}

@Serializable
internal data class BaseUpdatePacketWire(
    val type: String = "base",
)

internal fun BaseUpdatePacket.toWire() = BaseUpdatePacketWire(type)
internal fun BaseUpdatePacketWire.toPublic() = BaseUpdatePacket(type)

@Serializable
internal data class FlightUpdatePacketWire(
    val type: String = "flight",
    val basePosition: CoordinateWire,
    val basePositionUpdateTime: Long,
    val planePosition: CoordinateWire,
    val planePositionUpdateTime: Long,
    val flightRoute: List<CoordinateWire>,
    val flightRouteUpdateTime: Long,
    val plannedRoute: List<CoordinateWire>,
    val plannedRouteUpdateTime: Long,
)

internal fun FlightUpdatePacket.toWire() = FlightUpdatePacketWire(
    type = type,
    basePosition = basePosition.toWire(),
    basePositionUpdateTime = basePositionUpdateTime,
    planePosition = planePosition.toWire(),
    planePositionUpdateTime = planePositionUpdateTime,
    flightRoute = flightRoute.map { it.toWire() },
    flightRouteUpdateTime = flightRouteUpdateTime,
    plannedRoute = plannedRoute.map { it.toWire() },
    plannedRouteUpdateTime = plannedRouteUpdateTime,
)

internal fun FlightUpdatePacketWire.toPublic() = FlightUpdatePacket(
    type = type,
    basePosition = basePosition.toPublic(),
    basePositionUpdateTime = basePositionUpdateTime,
    planePosition = planePosition.toPublic(),
    planePositionUpdateTime = planePositionUpdateTime,
    flightRoute = flightRoute.map { it.toPublic() },
    flightRouteUpdateTime = flightRouteUpdateTime,
    plannedRoute = plannedRoute.map { it.toPublic() },
    plannedRouteUpdateTime = plannedRouteUpdateTime,
)

@Serializable
internal data class ConnectionUpdatePacketWire(
    val type: String = "connection",
    val baseConnectionState: ConnectionStateWire,
    val lastContactBaseStationTimestamp: Long,
    val planeConnectionState: ConnectionStateWire,
    val lastContactPlaneTimestamp: Long,
    val gpsConnectionBase: ConnectionStateWire,
    val gpsConnectionPlane: ConnectionStateWire,
    val barometerConnectionBase: ConnectionStateWire,
    val barometerConnectionPlane: ConnectionStateWire,
    val motorComConnectionPlane: ConnectionStateWire,
    val magnetometerConnectionPlane: ConnectionStateWire,
    val accelerometerConnectionPlane: ConnectionStateWire,
    val manualOverridePlane: Boolean,
    val flightState: FlightStateWire,
)

internal fun ConnectionUpdatePacket.toWire() = ConnectionUpdatePacketWire(
    type = type,
    baseConnectionState = baseConnectionState.toWire(),
    lastContactBaseStationTimestamp = lastContactBaseStationTimestamp,
    planeConnectionState = planeConnectionState.toWire(),
    lastContactPlaneTimestamp = lastContactPlaneTimestamp,
    gpsConnectionBase = gpsConnectionBase.toWire(),
    gpsConnectionPlane = gpsConnectionPlane.toWire(),
    barometerConnectionBase = barometerConnectionBase.toWire(),
    barometerConnectionPlane = barometerConnectionPlane.toWire(),
    motorComConnectionPlane = motorComConnectionPlane.toWire(),
    magnetometerConnectionPlane = magnetometerConnectionPlane.toWire(),
    accelerometerConnectionPlane = accelerometerConnectionPlane.toWire(),
    manualOverridePlane = manualOverridePlane,
    flightState = flightState.toWire(),
)

internal fun ConnectionUpdatePacketWire.toPublic() = ConnectionUpdatePacket(
    type = type,
    baseConnectionState = baseConnectionState.toPublic(),
    lastContactBaseStationTimestamp = lastContactBaseStationTimestamp,
    planeConnectionState = planeConnectionState.toPublic(),
    lastContactPlaneTimestamp = lastContactPlaneTimestamp,
    gpsConnectionBase = gpsConnectionBase.toPublic(),
    gpsConnectionPlane = gpsConnectionPlane.toPublic(),
    barometerConnectionBase = barometerConnectionBase.toPublic(),
    barometerConnectionPlane = barometerConnectionPlane.toPublic(),
    motorComConnectionPlane = motorComConnectionPlane.toPublic(),
    magnetometerConnectionPlane = magnetometerConnectionPlane.toPublic(),
    accelerometerConnectionPlane = accelerometerConnectionPlane.toPublic(),
    manualOverridePlane = manualOverridePlane,
    flightState = flightState.toPublic(),
)

/** Unlike [AreaDefinePacket] this carries no `type` field at all - it's not tagged on the wire. */
@Serializable
internal data class AreaDefinePacketWire(
    val shape: List<CoordinateWire>,
)

internal fun AreaDefinePacket.toWire() = AreaDefinePacketWire(shape.map { it.toWire() })
internal fun AreaDefinePacketWire.toPublic() = AreaDefinePacket(shape = shape.map { it.toPublic() })

@Serializable
internal data class SensorPacketWire(
    val type: String = "sensor",
    val barometerPressureBase: Float,
    val barometerPressurePlane: Float,
    val calculatedAltitude: Float,
    val headingPlane: Int,
)

internal fun SensorPacket.toWire() = SensorPacketWire(
    type = type,
    barometerPressureBase = barometerPressureBase,
    barometerPressurePlane = barometerPressurePlane,
    calculatedAltitude = calculatedAltitude,
    headingPlane = headingPlane,
)

internal fun SensorPacketWire.toPublic() = SensorPacket(
    type = type,
    barometerPressureBase = barometerPressureBase,
    barometerPressurePlane = barometerPressurePlane,
    calculatedAltitude = calculatedAltitude,
    headingPlane = headingPlane,
)

@Serializable
internal data class BatteryStatusPacketWire(
    val type: String = "battery",
    val baseBatteryPercentage: Int,
    val planeBatteryPercentage: Int,
)

internal fun BatteryStatusPacket.toWire() = BatteryStatusPacketWire(type, baseBatteryPercentage, planeBatteryPercentage)
internal fun BatteryStatusPacketWire.toPublic() = BatteryStatusPacket(type, baseBatteryPercentage, planeBatteryPercentage)

@Serializable
internal data class PlannedRoutePacketWire(
    val type: String = "plannedRoute",
    val route: List<CoordinateWire>,
)

internal fun PlannedRoutePacket.toWire() = PlannedRoutePacketWire(type, route.map { it.toWire() })
internal fun PlannedRoutePacketWire.toPublic() = PlannedRoutePacket(type, route.map { it.toPublic() })
