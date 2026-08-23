package de.universegame.auto_flight.app

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.longOrNull
import kotlin.time.ExperimentalTime
import kotlin.time.Instant

/**
 * Result of interpreting a single websocket text frame.
 */
sealed interface WebSocketEvent {
    /** Heartbeat reply - proof the base station link is alive. */
    data object Pong : WebSocketEvent

    /** A `connection` / `flight` / `sensor` packet was applied, yielding [state]. */
    data class StateUpdate(val state: AppState) : WebSocketEvent
}

/**
 * Pure functions that turn base-station websocket frames into [AppState] updates.
 *
 * Field shapes are intentionally treated as loosely typed JSON (like the original
 * web client did) because the firmware has historically sent coordinates as either
 * `[lat, lon]` arrays or `{lat, lon}` / `{latitude, longitude}` objects, and
 * timestamps as either epoch seconds/millis or ISO-8601 strings.
 */
internal val appJson = Json { ignoreUnknownKeys = true }

object PacketParser {

    private val json = appJson

    /** Strips BOM/NUL characters and surrounding whitespace from websocket payloads. */
    fun normalize(raw: String): String =
        raw.trimStart('\uFEFF').filterNot { it == '\u0000' }.trim()

    /**
     * Parses one websocket text frame. Returns `null` if the frame is empty or not
     * a recognizable packet (unknown/missing `type` field, malformed JSON, ...).
     */
    fun parse(raw: String, current: AppState): WebSocketEvent? {
        val normalized = normalize(raw)
        if (normalized.isEmpty()) return null
        if (normalized == "pong") return WebSocketEvent.Pong

        val element = try {
            json.parseToJsonElement(normalized)
        } catch (_: Exception) {
            return null
        }
        val obj = element as? JsonObject ?: return null

        return when (obj["type"]?.jsonPrimitive?.contentOrNull) {
            "flight" -> WebSocketEvent.StateUpdate(applyFlightPacket(current, obj))
            "connection" -> WebSocketEvent.StateUpdate(applyConnectionPacket(current, obj))
            "sensor" -> WebSocketEvent.StateUpdate(applySensorPacket(current, obj))
            else -> null
        }
    }

    fun applyFlightPacket(current: AppState, packet: JsonObject): AppState {
        val basePos = toCoordinate(packet["basePosition"])
        val planePos = toCoordinate(packet["planePosition"])

        current.basePosition = basePos ?: current.basePosition
        current.basePositionUpdateTime = toTimeT(packet["basePositionUpdateTime"])

        val plane = current.planes.getOrPut(DEFAULT_PLANE_ID) { PlaneInfo(id = DEFAULT_PLANE_ID) }
        plane.position = planePos ?: plane.position
        plane.positionUpdateTime = toTimeT(packet["planePositionUpdateTime"])
        plane.flightRoute = toRoute(packet["flightRoute"])
        plane.flightRouteUpdateTime = toTimeT(packet["flightRouteUpdateTime"])
        plane.plannedRoute = toRoute(packet["plannedRoute"])
        plane.plannedRouteUpdateTime = toTimeT(packet["plannedRouteUpdateTime"])

        return current
    }

    fun applyConnectionPacket(current: AppState, packet: JsonObject): AppState {
        parseConnectionState(packet["baseConnectionState"])?.let { current.connectionStateBaseStation = it }
        current.lastContactBaseStationTimestamp = toTimeT(packet["lastContactBaseStationTimestamp"])
        parseConnectionState(packet["gpsConnectionBase"])?.let { current.gpsConnectionBase = it }
        parseConnectionState(packet["barometerConnectionBase"])?.let { current.barometerConnectionBase = it }

        val plane = current.planes.getOrPut(DEFAULT_PLANE_ID) { PlaneInfo(id = DEFAULT_PLANE_ID) }
        parseConnectionState(packet["planeConnectionState"])?.let { plane.connectionState = it }
        plane.lastContactTimestamp = toTimeT(packet["lastContactPlaneTimestamp"])
        parseConnectionState(packet["gpsConnectionPlane"])?.let { plane.gpsConnection = it }
        parseConnectionState(packet["barometerConnectionPlane"])?.let { plane.barometerConnection = it }
        parseConnectionState(packet["motorComConnectionPlane"])?.let { plane.motorComConnection = it }
        parseConnectionState(packet["magnetometerConnectionPlane"])?.let { plane.magnetometerConnection = it }
        parseConnectionState(packet["accelerometerConnectionPlane"])?.let { plane.accelerometerConnection = it }
        toBoolean(packet["manualOverridePlane"])?.let { plane.manualOverride = it }
        parseFlightState(packet["flightState"])?.let { plane.flightState = it }

        return current
    }

    fun applySensorPacket(current: AppState, packet: JsonObject): AppState {
        val pressureBase = (packet["barometerPressureBase"] as? JsonPrimitive)?.doubleOrNull
        val pressurePlane = (packet["barometerPressurePlane"] as? JsonPrimitive)?.doubleOrNull
        val calculatedAltitude = (packet["calculatedAltitude"] as? JsonPrimitive)?.doubleOrNull
        val headingPlane = (packet["headingPlane"] as? JsonPrimitive)?.doubleOrNull

        current.pressureBase = pressureBase?.takeIf { it != 0.0 } ?: current.pressureBase

        val plane = current.planes.getOrPut(DEFAULT_PLANE_ID) { PlaneInfo(id = DEFAULT_PLANE_ID) }
        plane.pressure = pressurePlane?.takeIf { it != 0.0 } ?: plane.pressure
        plane.calculatedAltitude = calculatedAltitude?.takeIf { it != 0.0 } ?: plane.calculatedAltitude
        plane.heading = headingPlane ?: plane.heading

        return current
    }

    /** Normalizes Unix timestamps (seconds or millis) or ISO date strings into seconds since epoch. */
    @OptIn(ExperimentalTime::class)
    fun toTimeT(element: JsonElement?): Long? {
        val primitive = element as? JsonPrimitive ?: return null
        primitive.longOrNull?.let { return if (it > 1_000_000_000_000L) it / 1000 else it }
        primitive.doubleOrNull?.let { return (if (it > 1e12) it / 1000 else it).toLong() }
        val content = primitive.contentOrNull ?: return null
        return try {
            Instant.parse(content).epochSeconds
        } catch (_: Exception) {
            null
        }
    }

    /** Converts common coordinate payload shapes into a normalized [Coordinate], or `null` if invalid/sentinel. */
    fun toCoordinate(element: JsonElement?): Coordinate? {
        if (element == null) return null

        var coord: Coordinate? = null

        val array = element as? JsonArray
        if (array != null && array.size >= 2) {
            val lat = array[0].jsonPrimitive.doubleOrNull
            val lon = array[1].jsonPrimitive.doubleOrNull
            if (lat != null && lon != null) coord = Coordinate(latitude = lat, longitude = lon)
        }

        val obj = element as? JsonObject
        if (obj != null) {
            val lat = (obj["lat"] as? JsonPrimitive)?.doubleOrNull
            val lon = (obj["lon"] as? JsonPrimitive)?.doubleOrNull
            if (lat != null && lon != null) coord = Coordinate(latitude = lat, longitude = lon)

            val latitude = (obj["latitude"] as? JsonPrimitive)?.doubleOrNull
            val longitude = (obj["longitude"] as? JsonPrimitive)?.doubleOrNull
            if (latitude != null && longitude != null) coord = Coordinate(latitude = latitude, longitude = longitude)
        }

        if (coord != null && coord.latitude == 0.0 && coord.longitude == 0.0) coord = null
        if (coord != null && (coord.latitude < -200 || coord.longitude < -200)) coord = null

        return coord
    }

    private fun toRoute(element: JsonElement?): List<Coordinate>? {
        val array = element as? JsonArray ?: return null
        return array.mapNotNull { toCoordinate(it) }
    }

    private fun toBoolean(element: JsonElement?): Boolean? {
        val primitive = element as? JsonPrimitive ?: return null
        return when (primitive.contentOrNull) {
            "true" -> true
            "false" -> false
            else -> null
        }
    }

    /** Parses connection state values from either enum names or case-insensitive strings. */
    fun parseConnectionState(element: JsonElement?): ConnectionState? {
        val content = (element as? JsonPrimitive)?.contentOrNull ?: return null
        return ConnectionState.entries.firstOrNull { it.name.equals(content, ignoreCase = true) }
    }

    /** Parses flight state values from either enum names or case-insensitive strings. */
    fun parseFlightState(element: JsonElement?): FlightState? {
        val content = (element as? JsonPrimitive)?.contentOrNull ?: return null
        return FlightState.entries.firstOrNull { it.name.equals(content, ignoreCase = true) }
    }
}
