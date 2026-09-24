package de.universegame.auto_flight.app.wire

import de.universegame.auto_flight.app.Coordinate
import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.cbor.Cbor
import kotlinx.serialization.decodeFromByteArray
import kotlinx.serialization.encodeToByteArray

/**
 * Mirrors of the base-station <-> frontend wire protocol defined in
 * `base_station/components/frontend/FrontendPackets.hpp` and the shared types it builds on in
 * `shared_components/flight_data/types.hpp`. Field names/enum wire values follow the C++ structs
 * exactly; JSON keys are name-based (not positional), so the domain [Coordinate]'s field order
 * and `Double` precision are interchangeable with the wire's `float longitude; float latitude;`.
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

enum class RouteAlgorithm {
    BASIC,
    BOUSTROPHEDON
}

/** Mirrors `RouteSettings` (shared_components/flight_data/RouteData.hpp). */
data class RouteSettings(
    val routeAlgorithm: RouteAlgorithm,
    val overlapPercentage: UInt
)

/**
 * Common supertype of the `type`-tagged packets in FrontendPackets.hpp, matching the C++ structs'
 * `static constexpr const char* type` member.
 */
sealed interface FrontendPacket {
    val sourceId: UInt
}

/** Mirrors `BaseUpdatePacket`. */
data class BaseUpdatePacket(
    override val sourceId: UInt,
) : FrontendPacket

/** Mirrors `PositionUpdatePacket`. */
data class PositionUpdatePacket(
    override val sourceId: UInt,
    val position: Coordinate,
    val positionUpdateTime: Long
) : FrontendPacket

/** Mirrors `ConnectionUpdatePacket`. */
data class ConnectionUpdatePacket(
    override val sourceId: UInt,
    val gpsConnection: ConnectionState,
    val barometer: ConnectionState,
    val motorCom: ConnectionState,
    val magnetometer: ConnectionState,
    val accelerometer: ConnectionState,
    val manualOverride: Boolean,
    val flightState: FlightState
) : FrontendPacket

/** Mirrors `AreaDefinePacket`. Unlike the others it carries no `type` tag on the wire. */
data class AreaDefinePacket(
    override val sourceId: UInt,
    val shape: List<Coordinate>,
    val settings: RouteSettings
) : FrontendPacket

/** Mirrors `SensorPacket`. */
data class SensorPacket(
    override val sourceId: UInt,
    val barometerPressure: Float,
    val calculatedAltitude: Float,
    val headingPlane: Int,
) : FrontendPacket

/** Mirrors `BatteryStatusPacket`. */
data class BatteryStatusPacket(
    override val sourceId: UInt,
    val batteryPercentage: Int
) : FrontendPacket

/** Mirrors `PlannedRoutePacket`. */
data class PlannedRoutePacket(
    override val sourceId: UInt,
    val route: List<Coordinate>,
    val settings: RouteSettings,
    val hash: ULong,
) : FrontendPacket

/** Mirrors `PlannedRouteConfirmationPacket`. */
data class PlannedRouteConfirmationPacket(
    override val sourceId: UInt,
    val hash: ULong,
) : FrontendPacket

/**
 * The shared CBOR codec for the wire types above. Each packet type maps to its own dedicated
 * Bluetooth GATT characteristic (see `iosApp/Networking/ConnectionManager.swift`), so there's no
 * generic type-tagged stream to demultiplex - callers already know which concrete type a given
 * characteristic's payload decodes to, hence one `decodeX` function per type rather than a single
 * polymorphic `decode(ByteArray): FrontendPacket?`.
 *
 * Bytes cross the Kotlin/Swift boundary as hex strings, not `ByteArray`: Swift Export (Alpha) has
 * no bridge for *constructing* a Kotlin `ByteArray` from Swift (only reading an existing one), so
 * Swift has no way to hand raw bytes to a `ByteArray`-typed parameter. `String` is fully bridged
 * in both directions, so hex is the round-trippable common ground.
 */
@OptIn(ExperimentalSerializationApi::class, ExperimentalStdlibApi::class)
object FrontendPackets {
    private val cbor = Cbor { ignoreUnknownKeys = true }

    fun decodeBaseUpdate(hex: String): BaseUpdatePacket? =
        runCatching { cbor.decodeFromByteArray<BaseUpdatePacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodePositionUpdate(hex: String): PositionUpdatePacket? =
        runCatching { cbor.decodeFromByteArray<PositionUpdatePacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodeConnectionUpdate(hex: String): ConnectionUpdatePacket? =
        runCatching { cbor.decodeFromByteArray<ConnectionUpdatePacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodeAreaDefine(hex: String): AreaDefinePacket? =
        runCatching { cbor.decodeFromByteArray<AreaDefinePacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodeSensor(hex: String): SensorPacket? =
        runCatching { cbor.decodeFromByteArray<SensorPacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodeBatteryStatus(hex: String): BatteryStatusPacket? =
        runCatching { cbor.decodeFromByteArray<BatteryStatusPacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodePlannedRoute(hex: String): PlannedRoutePacket? =
        runCatching { cbor.decodeFromByteArray<PlannedRoutePacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    fun decodePlannedRouteConfirmation(hex: String): PlannedRouteConfirmationPacket? =
        runCatching { cbor.decodeFromByteArray<PlannedRouteConfirmationPacketWire>(hex.hexToByteArray()).toPublic() }.getOrNull()

    /** Encodes any [FrontendPacket] to its CBOR wire form, hex-encoded. */
    fun encode(packet: FrontendPacket): String = when (packet) {
        is BaseUpdatePacket -> cbor.encodeToByteArray(packet.toWire())
        is PositionUpdatePacket -> cbor.encodeToByteArray(packet.toWire())
        is ConnectionUpdatePacket -> cbor.encodeToByteArray(packet.toWire())
        is AreaDefinePacket -> cbor.encodeToByteArray(packet.toWire())
        is SensorPacket -> cbor.encodeToByteArray(packet.toWire())
        is BatteryStatusPacket -> cbor.encodeToByteArray(packet.toWire())
        is PlannedRoutePacket -> cbor.encodeToByteArray(packet.toWire())
        is PlannedRouteConfirmationPacket -> cbor.encodeToByteArray(packet.toWire())
    }.toHexString()
}
