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
 *
 * Every packet struct repeats the `BasePacket` fields (`timestamp`, `type`, `id`) since
 * NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE flattens them into the same CBOR map. `type` is kept as
 * its raw integer value here (the C++ enum has no string mapping); decoding throws on an unknown
 * or mismatching value, which the `runCatching` in [FrontendPackets] turns into `null`.
 */

@Serializable
internal data class CoordinateWire(val latitude: Double, val longitude: Double)

internal fun Coordinate.toWire() = CoordinateWire(latitude, longitude)
/** Throws on a NaN component, so the enclosing packet decodes to `null` as a whole. */
internal fun CoordinateWire.toPublic(): Coordinate {
    require(!latitude.isNaN() && !longitude.isNaN()) { "coordinate has NaN component: $this" }
    return Coordinate(latitude, longitude)
}

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
internal enum class RouteAlgorithmWire {
    @SerialName("basic") BASIC,
    @SerialName("boustrophedon") BOUSTROPHEDON,
}

internal fun RouteAlgorithm.toWire() = when (this) {
    RouteAlgorithm.BASIC -> RouteAlgorithmWire.BASIC
    RouteAlgorithm.BOUSTROPHEDON -> RouteAlgorithmWire.BOUSTROPHEDON
}

internal fun RouteAlgorithmWire.toPublic() = when (this) {
    RouteAlgorithmWire.BASIC -> RouteAlgorithm.BASIC
    RouteAlgorithmWire.BOUSTROPHEDON -> RouteAlgorithm.BOUSTROPHEDON
}

@Serializable
internal data class RouteSettingsWire(
    val routeAlgorithm: RouteAlgorithmWire,
    val overlapPercentage: Int,
)

internal fun RouteSettings.toWire() = RouteSettingsWire(routeAlgorithm.toWire(), overlapPercentage)
internal fun RouteSettingsWire.toPublic() = RouteSettings(routeAlgorithm.toPublic(), overlapPercentage)

private fun packetTypeOf(wireValue: Int) =
    requireNotNull(PacketType.fromWireValue(wireValue)) { "unknown packet type $wireValue" }

private fun Int.requireType(expected: PacketType) =
    require(this == expected.wireValue) { "expected packet type ${expected.wireValue}, got $this" }

@Serializable
internal data class BasePacketWire(
    val timestamp: Long,
    val type: Int,
    val id: UInt,
)

internal fun BasePacket.toWire() = BasePacketWire(timestamp, type.wireValue, id)
internal fun BasePacketWire.toPublic() = BasePacket(type = packetTypeOf(type), id = id, timestamp = timestamp)

@Serializable
internal data class SensorUpdateWire(
    val timestamp: Long,
    val type: Int,
    val id: UInt,
    val pressure: Float,
    val heading: Int,
    val batteryPercent: Int,
)

internal fun SensorUpdate.toWire() = SensorUpdateWire(
    timestamp = timestamp,
    type = type.wireValue,
    id = id,
    pressure = pressure,
    heading = heading,
    batteryPercent = batteryPercent,
)

internal fun SensorUpdateWire.toPublic(): SensorUpdate {
    type.requireType(PacketType.SENSOR_UPDATE)
    return SensorUpdate(
        timestamp = timestamp,
        id = id,
        pressure = pressure,
        heading = heading,
        batteryPercent = batteryPercent,
    )
}

@Serializable
internal data class PositionUpdateWire(
    val timestamp: Long,
    val type: Int,
    val id: UInt,
    val position: CoordinateWire,
)

internal fun PositionUpdate.toWire() = PositionUpdateWire(timestamp, type.wireValue, id, position.toWire())

internal fun PositionUpdateWire.toPublic(): PositionUpdate {
    type.requireType(PacketType.POSITION)
    return PositionUpdate(id = id, position = position.toPublic(), timestamp = timestamp)
}

@Serializable
internal data class ComponentStatusWire(
    val timestamp: Long,
    val type: Int,
    val id: UInt,
    val gps: ConnectionStateWire,
    val barometer: ConnectionStateWire,
    val motorControl: ConnectionStateWire,
    val magnetometer: ConnectionStateWire,
    val accelerometer: ConnectionStateWire,
    val battery: ConnectionStateWire,
    val manualOverride: Boolean,
    val flightState: FlightStateWire,
)

internal fun ComponentStatus.toWire() = ComponentStatusWire(
    timestamp = timestamp,
    type = type.wireValue,
    id = id,
    gps = gps.toWire(),
    barometer = barometer.toWire(),
    motorControl = motorControl.toWire(),
    magnetometer = magnetometer.toWire(),
    accelerometer = accelerometer.toWire(),
    battery = battery.toWire(),
    manualOverride = manualOverride,
    flightState = flightState.toWire(),
)

internal fun ComponentStatusWire.toPublic(): ComponentStatus {
    type.requireType(PacketType.COMPONENT_STATUS)
    return ComponentStatus(
        timestamp = timestamp,
        id = id,
        gps = gps.toPublic(),
        barometer = barometer.toPublic(),
        motorControl = motorControl.toPublic(),
        magnetometer = magnetometer.toPublic(),
        accelerometer = accelerometer.toPublic(),
        battery = battery.toPublic(),
        manualOverride = manualOverride,
        flightState = flightState.toPublic(),
    )
}

@Serializable
internal data class PlannedAreaPacketWire(
    val timestamp: Long,
    val type: Int,
    val id: UInt,
    val shape: List<CoordinateWire>,
    val settings: RouteSettingsWire,
)

internal fun PlannedAreaPacket.toWire() =
    PlannedAreaPacketWire(timestamp, type.wireValue, id, shape.map { it.toWire() }, settings.toWire())

internal fun PlannedAreaPacketWire.toPublic(): PlannedAreaPacket {
    type.requireType(PacketType.PLANNED_AREA)
    return PlannedAreaPacket(id = id, shape = shape.map { it.toPublic() }, settings = settings.toPublic(), timestamp = timestamp)
}

@Serializable
internal data class FlightHistoryPacketWire(
    val timestamp: Long,
    val type: Int,
    val id: UInt,
    val history: List<CoordinateWire>,
)

internal fun FlightHistoryPacket.toWire() =
    FlightHistoryPacketWire(timestamp, type.wireValue, id, history.map { it.toWire() })

internal fun FlightHistoryPacketWire.toPublic(): FlightHistoryPacket {
    type.requireType(PacketType.ROUTE_HISTORY)
    return FlightHistoryPacket(id = id, history = history.map { it.toPublic() }, timestamp = timestamp)
}

/** `hash` is optional since the C++ side currently doesn't serialize it, see [PlannedRoutePacket]. */
@Serializable
internal data class PlannedRoutePacketWire(
    val timestamp: Long,
    val type: Int,
    val id: UInt,
    val route: List<CoordinateWire>,
    val settings: RouteSettingsWire,
    val hash: ULong = 0u,
)

internal fun PlannedRoutePacket.toWire() =
    PlannedRoutePacketWire(timestamp, type.wireValue, id, route.map { it.toWire() }, settings.toWire(), hash)

internal fun PlannedRoutePacketWire.toPublic(): PlannedRoutePacket {
    type.requireType(PacketType.PLANNED_ROUTE)
    return PlannedRoutePacket(
        id = id,
        route = route.map { it.toPublic() },
        settings = settings.toPublic(),
        hash = hash,
        timestamp = timestamp,
    )
}

@Serializable
internal data class PlannedRouteConfirmationPacketWire(
    val timestamp: Long,
    val type: Int,
    val id: UInt,
    val hash: ULong,
)

internal fun PlannedRouteConfirmationPacket.toWire() =
    PlannedRouteConfirmationPacketWire(timestamp, type.wireValue, id, hash)

internal fun PlannedRouteConfirmationPacketWire.toPublic(): PlannedRouteConfirmationPacket {
    type.requireType(PacketType.PLANNED_ROUTE_CONFIRMATION)
    return PlannedRouteConfirmationPacket(id = id, hash = hash, timestamp = timestamp)
}
