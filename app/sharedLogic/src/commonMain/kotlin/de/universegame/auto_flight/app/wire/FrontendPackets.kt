package de.universegame.auto_flight.app.wire

import de.universegame.auto_flight.app.Coordinate
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.decodeFromJsonElement
import kotlinx.serialization.json.jsonPrimitive

/**
 * kotlinx.serialization mirrors of the base-station <-> frontend wire protocol defined in
 * `base_station/components/frontend/FrontendPackets.hpp` and the shared types it builds on in
 * `shared_components/flight_data/types.hpp`. Field names/enum wire values follow the C++ structs
 * exactly; JSON keys are name-based (not positional), so the domain [Coordinate]'s field order
 * and `Double` precision are interchangeable with the wire's `float longitude; float latitude;`.
 */

typealias Route = List<Coordinate>
typealias FlightRoute = Route
typealias PlannedRoute = Route

/** Mirrors `ConnectionState` (shared_components/flight_data/types.hpp); wire values are the NLOHMANN_JSON_SERIALIZE_ENUM strings. */
@Serializable
enum class ConnectionState {
    @SerialName("connecting") CONNECTING,
    @SerialName("connected") CONNECTED,
}

/** Mirrors `FlightState` (shared_components/flight_data/types.hpp); wire values are the NLOHMANN_JSON_SERIALIZE_ENUM strings. */
@Serializable
enum class FlightState {
    @SerialName("planning") PLANNING,
    @SerialName("planned") PLANNED,
    @SerialName("flying") FLYING,
    @SerialName("returning") RETURNING,
}

/**
 * Common supertype of the `type`-tagged packets in FrontendPackets.hpp. Not itself `@Serializable`
 * (each variant carries its own literal `type` field exactly like the C++ struct's
 * `static constexpr const char* type` member, so there's no room for a class-discriminator on top
 * of it) - encode/decode via the concrete type, or via [FrontendPackets.encode]/[FrontendPackets.decode]
 * when the concrete type isn't known ahead of time.
 */
sealed interface FrontendPacket

/** Mirrors `BaseUpdatePacket`. */
@Serializable
data class BaseUpdatePacket(
    val type: String = "base",
) : FrontendPacket

/** Mirrors `FlightUpdatePacket`. */
@Serializable
data class FlightUpdatePacket(
    val type: String = "flight",
    val basePosition: Coordinate,
    val basePositionUpdateTime: Long,
    val planePosition: Coordinate,
    val planePositionUpdateTime: Long,
    val flightRoute: FlightRoute,
    val flightRouteUpdateTime: Long,
    val plannedRoute: PlannedRoute,
    val plannedRouteUpdateTime: Long,
) : FrontendPacket

/** Mirrors `ConnectionUpdatePacket`. */
@Serializable
data class ConnectionUpdatePacket(
    val type: String = "connection",
    val baseConnectionState: ConnectionState,
    val lastContactBaseStationTimestamp: Long,
    val planeConnectionState: ConnectionState,
    val lastContactPlaneTimestamp: Long,
    val gpsConnectionBase: ConnectionState,
    val gpsConnectionPlane: ConnectionState,
    val barometerConnectionBase: ConnectionState,
    val barometerConnectionPlane: ConnectionState,
    val motorComConnectionPlane: ConnectionState,
    val magnetometerConnectionPlane: ConnectionState,
    val accelerometerConnectionPlane: ConnectionState,
    val manualOverridePlane: Boolean,
    val flightState: FlightState,
) : FrontendPacket

/** Mirrors `AreaDefinePacket`. Unlike the others it carries no `type` tag on the wire. */
@Serializable
data class AreaDefinePacket(
    val shape: List<Coordinate>,
) : FrontendPacket

/** Mirrors `SensorPacket`. */
@Serializable
data class SensorPacket(
    val type: String = "sensor",
    val barometerPressureBase: Float,
    val barometerPressurePlane: Float,
    val calculatedAltitude: Float,
    val headingPlane: Int,
) : FrontendPacket

/** Mirrors `BatteryStatusPacket`. */
@Serializable
data class BatteryStatusPacket(
    val type: String = "battery",
    val baseBatteryPercentage: Int,
    val planeBatteryPercentage: Int,
) : FrontendPacket

/** Mirrors `PlannedRoutePacket`. */
@Serializable
data class PlannedRoutePacket(
    val type: String = "plannedRoute",
    val route: List<Coordinate>,
) : FrontendPacket

/** Encodes/decodes [FrontendPacket]s by dispatching on the `type` field, for callers that receive a heterogeneous stream of packets. */
object FrontendPackets {
    private val json = Json { ignoreUnknownKeys = true }

    /** Decodes one JSON object into the [FrontendPacket] matching its `type` field, or `null` if malformed/unrecognized. */
    fun decode(raw: String): FrontendPacket? {
        val obj = try {
            json.parseToJsonElement(raw) as? JsonObject
        } catch (_: Exception) {
            null
        } ?: return null

        return when (obj["type"]?.jsonPrimitive?.contentOrNull) {
            "base" -> json.decodeFromJsonElement<BaseUpdatePacket>(obj)
            "flight" -> json.decodeFromJsonElement<FlightUpdatePacket>(obj)
            "connection" -> json.decodeFromJsonElement<ConnectionUpdatePacket>(obj)
            "sensor" -> json.decodeFromJsonElement<SensorPacket>(obj)
            "battery" -> json.decodeFromJsonElement<BatteryStatusPacket>(obj)
            "plannedRoute" -> json.decodeFromJsonElement<PlannedRoutePacket>(obj)
            else -> null
        }
    }

    /** Encodes any [FrontendPacket] to its JSON wire form. */
    fun encode(packet: FrontendPacket): String = when (packet) {
        is BaseUpdatePacket -> json.encodeToString(packet)
        is FlightUpdatePacket -> json.encodeToString(packet)
        is ConnectionUpdatePacket -> json.encodeToString(packet)
        is AreaDefinePacket -> json.encodeToString(packet)
        is SensorPacket -> json.encodeToString(packet)
        is BatteryStatusPacket -> json.encodeToString(packet)
        is PlannedRoutePacket -> json.encodeToString(packet)
    }
}
