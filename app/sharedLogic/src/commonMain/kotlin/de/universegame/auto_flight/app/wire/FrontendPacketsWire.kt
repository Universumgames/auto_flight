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
    val sourceId: UInt
)

internal fun BaseUpdatePacket.toWire() = BaseUpdatePacketWire(sourceId)
internal fun BaseUpdatePacketWire.toPublic() = BaseUpdatePacket(sourceId)

@Serializable
internal data class PositionUpdatePacketWire (
    val sourceId: UInt,
    val position: CoordinateWire,
    val positionUpdateTime: Long
)

internal fun PositionUpdatePacket.toWire() = PositionUpdatePacketWire(
    sourceId = sourceId,
    position = position.toWire(),
    positionUpdateTime = positionUpdateTime
)

internal fun PositionUpdatePacketWire.toPublic() = PositionUpdatePacket(
    sourceId = sourceId,
    position = position.toPublic(),
    positionUpdateTime = positionUpdateTime
)

@Serializable
internal data class ConnectionUpdatePacketWire(
    val sourceId: UInt,
    val gpsConnection: ConnectionStateWire,
    val barometer: ConnectionStateWire,
    val motorCom: ConnectionStateWire,
    val magnetometer: ConnectionStateWire,
    val accelerometer: ConnectionStateWire,
    val manualOverride: Boolean,
    val flightState: FlightStateWire
)

internal fun ConnectionUpdatePacket.toWire() = ConnectionUpdatePacketWire(
    sourceId = sourceId,
    gpsConnection = gpsConnection.toWire(),
    barometer = barometer.toWire(),
    motorCom = motorCom.toWire(),
    magnetometer = magnetometer.toWire(),
    accelerometer = accelerometer.toWire(),
    manualOverride = manualOverride,
    flightState = flightState.toWire(),
)

internal fun ConnectionUpdatePacketWire.toPublic() = ConnectionUpdatePacket(
    sourceId = sourceId,
    gpsConnection = gpsConnection.toPublic(),
    barometer = barometer.toPublic(),
    motorCom = motorCom.toPublic(),
    magnetometer = magnetometer.toPublic(),
    accelerometer = accelerometer.toPublic(),
    manualOverride = manualOverride,
    flightState = flightState.toPublic(),
)

/** Unlike [AreaDefinePacket] this carries no `type` field at all - it's not tagged on the wire. */
@Serializable
internal data class AreaDefinePacketWire(
    val sourceId: UInt,
    val shape: List<CoordinateWire>,
)

internal fun AreaDefinePacket.toWire() = AreaDefinePacketWire(sourceId, shape.map { it.toWire() })
internal fun AreaDefinePacketWire.toPublic() = AreaDefinePacket(sourceId = sourceId, shape = shape.map { it.toPublic() })

@Serializable
internal data class SensorPacketWire(
    val sourceId: UInt,
    val barometerPressure: Float,
    val calculatedAltitude: Float,
    val headingPlane: Int,
)

internal fun SensorPacket.toWire() = SensorPacketWire(
    sourceId = sourceId,
    barometerPressure = barometerPressure,
    calculatedAltitude = calculatedAltitude,
    headingPlane = headingPlane,
)

internal fun SensorPacketWire.toPublic() = SensorPacket(
    sourceId = sourceId,
    barometerPressure = barometerPressure,
    calculatedAltitude = calculatedAltitude,
    headingPlane = headingPlane
)

@Serializable
internal data class BatteryStatusPacketWire(
    val sourceId: UInt,
    val batteryPercentage: Int
)

internal fun BatteryStatusPacket.toWire() = BatteryStatusPacketWire(sourceId = sourceId, batteryPercentage = batteryPercentage)
internal fun BatteryStatusPacketWire.toPublic() = BatteryStatusPacket(sourceId, batteryPercentage)

@Serializable
internal data class PlannedRoutePacketWire(
    val sourceId: UInt,
    val route: List<CoordinateWire>,
)

internal fun PlannedRoutePacket.toWire() = PlannedRoutePacketWire(sourceId, route.map { it.toWire() })
internal fun PlannedRoutePacketWire.toPublic() = PlannedRoutePacket(sourceId, route.map { it.toPublic() })
