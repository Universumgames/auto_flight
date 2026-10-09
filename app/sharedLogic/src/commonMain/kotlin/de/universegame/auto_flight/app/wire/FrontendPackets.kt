package de.universegame.auto_flight.app.wire

import de.universegame.auto_flight.app.Coordinate
import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.cbor.Cbor
import kotlinx.serialization.decodeFromByteArray
import kotlinx.serialization.encodeToByteArray
import kotlin.time.Clock

/**
 * Mirrors of the flight communication packets defined in
 * `shared_components/flight_com/packets/` and the shared types they build on in
 * `shared_components/flight_data/types.hpp` / `RouteData.hpp`. Field names/enum wire values follow
 * the C++ structs exactly; JSON keys are name-based (not positional), so the domain [Coordinate]'s
 * field order and `Double` precision are interchangeable with the wire's
 * `float longitude; float latitude;`.
 *
 * These public types are plain data holders, not `@Serializable` themselves - the actual CBOR
 * codec lives in [FrontendPackets] below, backed by `internal` mirror types in
 * FrontendPacketsWire.kt. A public `@Serializable` class gets a public `KSerializer`-returning
 * `serializer()` accessor, which forces Swift Export to bridge kotlinx-serialization-core to
 * describe it - and doing so currently trips a Swift Export Alpha compiler bug that double-emits
 * `CompositeDecoder`/`CompositeEncoder`/`Encoder` default members (`decodeSequentially`,
 * `shouldEncodeElementDefault`, `encodeNotNullMark`) as invalid redeclarations. Keeping the
 * `@Serializable` annotations off any `public` type avoids that entirely, since Swift Export only
 * ever sees public API - `internal` is enough for that, it doesn't need to be `private`/same-file.
 */

typealias Route = List<Coordinate>
typealias FlightRoute = Route
typealias PlannedRoute = Route

/** Mirrors `ConnectionState` (shared_components/flight_data/types.hpp); wire values are the NLOHMANN_JSON_SERIALIZE_ENUM strings ("connecting", "connected"). */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
}

/** Mirrors `FlightState` (shared_components/flight_data/types.hpp); wire values are the NLOHMANN_JSON_SERIALIZE_ENUM strings ("planning", "planned", "flying", "returning"). */
enum class FlightState {
    PLANNING,
    PLANNED,
    FLYING,
    RETURNING,
}

/** Mirrors `RouteAlgorithm` (shared_components/flight_data/types.hpp); wire values are the NLOHMANN_JSON_SERIALIZE_ENUM strings ("basic", "boustrophedon"). */
enum class RouteAlgorithm {
    BASIC,
    BOUSTROPHEDON
}

/** Mirrors `RouteSettings` (shared_components/flight_data/RouteData.hpp). */
data class RouteSettings(
    val routeAlgorithm: RouteAlgorithm,
    val overlapPercentage: Int
)

/**
 * Mirrors `PacketType` (flight_com/packets/base.hpp). It has no NLOHMANN_JSON_SERIALIZE_ENUM, so
 * on the wire it's the plain underlying `uint8_t` [wireValue].
 */
enum class PacketType(val wireValue: Int) {
    // sensors
    SENSOR_UPDATE(0x10),
    POSITION(0x11),
    COMPONENT_STATUS(0x12),

    // flight data
    /** the planned route by the plane to cover a specified area */
    PLANNED_ROUTE(0x30),
    /** the area the plane has to cover */
    PLANNED_AREA(0x31),
    /** the complete history of the plane's route since takeoff */
    ROUTE_HISTORY(0x32),
    /** requesting the complete history of the planes route since takeoff, use with caution */
    ROUTE_HISTORY_REQUEST(0x33),
    /** confirmation of the planned route by the base station */
    PLANNED_ROUTE_CONFIRMATION(0x34);

    companion object {
        fun fromWireValue(value: Int): PacketType? = entries.firstOrNull { it.wireValue == value }
    }
}

/**
 * Common supertype of all packets, mirroring the fields every C++ packet inherits from
 * `BasePacket`: the send [timestamp] (`time_t`, seconds), the packet [type] and the sender's
 * device [id]. Every constructor takes `timestamp` last and defaults it to [currentTimestamp].
 */
sealed interface FrontendPacket {
    val timestamp: Long
    val type: PacketType
    val id: UInt

    /** Alias for [id]: the device id of the packet's sender. */
    val sourceId: UInt get() = id
}

/** Current time as `time_t`-compatible epoch seconds, the default [FrontendPacket.timestamp]. */
fun currentTimestamp(): Long = Clock.System.now().epochSeconds

/**
 * Mirrors a bare `BasePacket` - a packet without payload, e.g. [PacketType.ROUTE_HISTORY_REQUEST].
 * Unlike the other packets its [type] isn't fixed.
 */
data class BasePacket(
    override val type: PacketType,
    override val id: UInt,
    override val timestamp: Long = currentTimestamp(),
) : FrontendPacket

/** Mirrors `SensorUpdate` (packets/sensor.hpp). */
data class SensorUpdate(
    override val id: UInt,
    /** Pressure in hPa */
    val pressure: Float,
    /** Compass heading in degrees [0, 360), rounded to the nearest int, 0 = magnetic north */
    val heading: Int,
    /** Battery percentage (0-100) */
    val batteryPercent: Int,
    override val timestamp: Long = currentTimestamp(),
) : FrontendPacket {
    override val type get() = PacketType.SENSOR_UPDATE
}

/** Mirrors `PositionUpdate` (packets/position.hpp). */
data class PositionUpdate(
    override val id: UInt,
    /** GPS position */
    val position: Coordinate,
    override val timestamp: Long = currentTimestamp(),
) : FrontendPacket {
    override val type get() = PacketType.POSITION
}

/** Mirrors `ComponentStatus` (packets/component.hpp). */
data class ComponentStatus(
    override val id: UInt,
    val gps: ConnectionState,
    val barometer: ConnectionState,
    val motorControl: ConnectionState,
    val magnetometer: ConnectionState,
    val accelerometer: ConnectionState,
    val battery: ConnectionState,
    val manualOverride: Boolean,
    val flightState: FlightState,
    override val timestamp: Long = currentTimestamp(),
) : FrontendPacket {
    override val type get() = PacketType.COMPONENT_STATUS
}

/** Mirrors `PlannedAreaPacket` (packets/area.hpp). */
data class PlannedAreaPacket(
    override val id: UInt,
    val shape: List<Coordinate>,
    val settings: RouteSettings,
    override val timestamp: Long = currentTimestamp(),
) : FrontendPacket {
    override val type get() = PacketType.PLANNED_AREA
}

/** Mirrors `FlightHistoryPacket` (packets/history.hpp). */
data class FlightHistoryPacket(
    override val id: UInt,
    val history: FlightRoute,
    override val timestamp: Long = currentTimestamp(),
) : FrontendPacket {
    override val type get() = PacketType.ROUTE_HISTORY
}

/**
 * Mirrors `PlannedRoutePacket` (packets/route.hpp). Note the C++ struct has a `hash` member but
 * leaves it out of its NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE field list, so it's currently never
 * on the wire and decodes as `0`.
 */
data class PlannedRoutePacket(
    override val id: UInt,
    val route: PlannedRoute,
    val settings: RouteSettings,
    val hash: ULong = 0u,
    override val timestamp: Long = currentTimestamp(),
) : FrontendPacket {
    override val type get() = PacketType.PLANNED_ROUTE

    val routeHash: ULong get() = hash
}

/** Mirrors `PlannedRouteConfirmationPacket` (packets/route.hpp). */
data class PlannedRouteConfirmationPacket(
    override val id: UInt,
    val hash: ULong,
    override val timestamp: Long = currentTimestamp(),
) : FrontendPacket {
    override val type get() = PacketType.PLANNED_ROUTE_CONFIRMATION
}

/**
 * The shared CBOR codec for the wire types above. Callers that already know which concrete type
 * a payload decodes to (e.g. one Bluetooth GATT characteristic per packet type, see
 * `iosApp/Networking/ConnectionManager.swift`) can use the typed `decodeX` functions; since every
 * packet carries its `type` on the wire, [decode] can also demultiplex an arbitrary payload.
 *
 * Bytes cross the Kotlin/Swift boundary as hex strings, not `ByteArray`: Swift Export (Alpha) has
 * no bridge for *constructing* a Kotlin `ByteArray` from Swift (only reading an existing one), so
 * Swift has no way to hand raw bytes to a `ByteArray`-typed parameter. `String` is fully bridged
 * in both directions, so hex is the round-trippable common ground.
 */
@OptIn(ExperimentalSerializationApi::class, ExperimentalStdlibApi::class)
object FrontendPackets {
    private val cbor = Cbor { ignoreUnknownKeys = true }

    /** Decodes any packet, dispatching on its wire `type`; `null` if malformed or of unknown type. */
    fun decode(hex: String): FrontendPacket? {
        val base = decodeBase(hex) ?: return null
        return when (base.type) {
            PacketType.SENSOR_UPDATE -> decodeSensorUpdate(hex)
            PacketType.POSITION -> decodePositionUpdate(hex)
            PacketType.COMPONENT_STATUS -> decodeComponentStatus(hex)
            PacketType.PLANNED_ROUTE -> decodePlannedRoute(hex)
            PacketType.PLANNED_AREA -> decodePlannedArea(hex)
            PacketType.ROUTE_HISTORY -> decodeFlightHistory(hex)
            PacketType.ROUTE_HISTORY_REQUEST -> base
            PacketType.PLANNED_ROUTE_CONFIRMATION -> decodePlannedRouteConfirmation(hex)
        }
    }

    fun decodeBase(hex: String): BasePacket? =
        runCatching { cbor.decodeFromByteArray<BasePacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodeSensorUpdate(hex: String): SensorUpdate? =
        runCatching { cbor.decodeFromByteArray<SensorUpdateWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodePositionUpdate(hex: String): PositionUpdate? =
        runCatching { cbor.decodeFromByteArray<PositionUpdateWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodeComponentStatus(hex: String): ComponentStatus? =
        runCatching { cbor.decodeFromByteArray<ComponentStatusWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodePlannedArea(hex: String): PlannedAreaPacket? =
        runCatching { cbor.decodeFromByteArray<PlannedAreaPacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodeFlightHistory(hex: String): FlightHistoryPacket? =
        runCatching { cbor.decodeFromByteArray<FlightHistoryPacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodePlannedRoute(hex: String): PlannedRoutePacket? =
        runCatching { cbor.decodeFromByteArray<PlannedRoutePacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodePlannedRouteConfirmation(hex: String): PlannedRouteConfirmationPacket? =
        runCatching { cbor.decodeFromByteArray<PlannedRouteConfirmationPacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    /** Encodes any [FrontendPacket] to its CBOR wire form, hex-encoded. */
    fun encode(packet: FrontendPacket): String = when (packet) {
        is BasePacket -> cbor.encodeToByteArray(packet.toWire())
        is SensorUpdate -> cbor.encodeToByteArray(packet.toWire())
        is PositionUpdate -> cbor.encodeToByteArray(packet.toWire())
        is ComponentStatus -> cbor.encodeToByteArray(packet.toWire())
        is PlannedAreaPacket -> cbor.encodeToByteArray(packet.toWire())
        is FlightHistoryPacket -> cbor.encodeToByteArray(packet.toWire())
        is PlannedRoutePacket -> cbor.encodeToByteArray(packet.toWire())
        is PlannedRouteConfirmationPacket -> cbor.encodeToByteArray(packet.toWire())
    }.toHexString()
}
