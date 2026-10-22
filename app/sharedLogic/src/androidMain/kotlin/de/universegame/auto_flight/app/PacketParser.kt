package de.universegame.auto_flight.app

import de.universegame.auto_flight.app.wire.ComponentStatus
import de.universegame.auto_flight.app.wire.FlightPacket
import de.universegame.auto_flight.app.wire.FrontendPacket
import de.universegame.auto_flight.app.wire.FrontendPackets
import de.universegame.auto_flight.app.wire.PlannedAreaPacket
import de.universegame.auto_flight.app.wire.PlannedRoutePacket
import de.universegame.auto_flight.app.wire.PositionUpdate
import de.universegame.auto_flight.app.wire.SensorUpdate
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.jsonPrimitive

/**
 * Result of interpreting a single websocket frame.
 */
sealed interface WebSocketEvent {
    /** Heartbeat reply - proof the base station link is alive. */
    data object Pong : WebSocketEvent

    /** A packet was applied, yielding [state]. */
    data class StateUpdate(val state: AppState) : WebSocketEvent
}

/** Only used for the JSON bodies of the HTTP `/api/area` and `/api/route` endpoints. */
internal val appJson = Json { ignoreUnknownKeys = true }

/**
 * Turns base-station frames into [AppState] updates. Mirrors the iOS app's
 * `PacketParsing.swift`: frames are the CBOR packets generated from `packets.jtd.json`,
 * decoded with [FrontendPackets], and each packet updates either the base station or
 * the plane its [FrontendPacket.sourceId] names.
 */
@OptIn(ExperimentalStdlibApi::class)
object PacketParser {

    /** Strips BOM/NUL characters and surrounding whitespace from websocket payloads. */
    fun normalize(raw: String): String =
        raw.trimStart('﻿').filterNot { it == '\u0000' }.trim()

    /**
     * Parses one text frame: either the heartbeat `pong` or a hex-encoded CBOR packet.
     * Returns `null` if the frame is empty or not a recognizable packet.
     */
    fun parse(raw: String, current: AppState): WebSocketEvent? {
        val normalized = normalize(raw)
        if (normalized.isEmpty()) return null
        if (normalized == "pong") return WebSocketEvent.Pong
        val packet = FrontendPackets.decode(normalized) ?: return null
        return apply(current, packet)?.let { WebSocketEvent.StateUpdate(it) }
    }

    /** Parses one binary frame holding a raw CBOR packet. */
    fun parse(bytes: ByteArray, current: AppState): WebSocketEvent? {
        if (bytes.isEmpty()) return null
        val packet = FrontendPackets.decode(bytes.toHexString()) ?: return null
        return apply(current, packet)?.let { WebSocketEvent.StateUpdate(it) }
    }

    /** Applies [packet] to [current]; `null` for packet types that don't affect the app state. */
    fun apply(current: AppState, packet: FrontendPacket): AppState? = when (packet) {
        is PositionUpdate -> applyPositionPacket(current, packet)
        is ComponentStatus -> applyConnectionPacket(current, packet)
        is SensorUpdate -> applySensorPacket(current, packet)
        is PlannedRoutePacket -> applyRoutePlannedPacket(current, packet)
        is PlannedAreaPacket -> applyAreaDefinePacket(current, packet)
        is FlightPacket -> applyFlightDataPacket(current, packet)
        else -> null
    }

    /** Returns the plane's existing entry in [AppState.planes], creating and inserting one if absent. */
    fun plane(current: AppState, id: PlaneID): PlaneInfo =
        current.planes.getOrPut(id) { PlaneInfo(id = id) }

    fun applyPositionPacket(current: AppState, packet: PositionUpdate): AppState {
        val position = validCoordinate(packet.position)
        if (packet.sourceId == BASE_ID) {
            current.basePosition = position ?: current.basePosition
            current.basePositionUpdateTime = packet.timestamp
        } else {
            val plane = plane(current, packet.sourceId)
            plane.position = position ?: plane.position
            plane.positionUpdateTime = packet.timestamp
        }
        return current
    }

    fun applyConnectionPacket(current: AppState, packet: ComponentStatus): AppState {
        if (packet.sourceId == BASE_ID) {
            current.gpsConnectionBase = packet.gps
            current.barometerConnectionBase = packet.barometer
        } else {
            val plane = plane(current, packet.sourceId)
            plane.connectionState = ConnectionState.CONNECTED
            plane.gpsConnection = packet.gps
            plane.barometerConnection = packet.barometer
            plane.motorComConnection = packet.motorControl
            plane.magnetometerConnection = packet.magnetometer
            plane.accelerometerConnection = packet.accelerometer
            plane.batteryConnection = packet.battery
            plane.manualOverride = packet.manualOverride
            plane.flightState = packet.flightState
        }
        return current
    }

    fun applySensorPacket(current: AppState, packet: SensorUpdate): AppState {
        if (packet.sourceId == BASE_ID) {
            current.pressureBase = packet.pressure.toDouble()
            current.batteryPercentageBase = packet.batteryPercent
        } else {
            val plane = plane(current, packet.sourceId)
            plane.pressure = packet.pressure.toDouble()
            plane.heading = packet.heading.toDouble()
            plane.batteryPercentage = packet.batteryPercent
        }
        return current
    }

    fun applyRoutePlannedPacket(current: AppState, packet: PlannedRoutePacket): AppState {
        if (packet.sourceId == BASE_ID) return current
        val plane = plane(current, packet.sourceId)
        plane.plannedRoute = packet.route
        plane.plannedRouteUpdateTime = packet.timestamp
        return current
    }

    fun applyAreaDefinePacket(current: AppState, packet: PlannedAreaPacket): AppState {
        if (packet.sourceId == BASE_ID) return current
        plane(current, packet.sourceId).area = AreaData(areaPoints = packet.shape, settings = packet.settings)
        return current
    }

    fun applyFlightDataPacket(current: AppState, packet: FlightPacket): AppState {
        if (packet.sourceId == BASE_ID) return current
        plane(current, packet.sourceId).nextRouteWaypoint = validCoordinate(packet.nextWaypoint)
        return current
    }

    /** Returns `null` for the (0,0) sentinel and coordinates below -200 (e.g. COORDINATE_INVALID_VALUE). */
    private fun validCoordinate(coord: Coordinate): Coordinate? {
        if (coord.latitude == 0.0 && coord.longitude == 0.0) return null
        if (coord.latitude < -200 || coord.longitude < -200) return null
        return coord
    }

    /**
     * Converts the coordinate shapes of the HTTP endpoints' JSON bodies (`[lat, lon]`,
     * `{lat, lon}` or `{latitude, longitude}`) into a [Coordinate], or `null` if invalid/sentinel.
     */
    fun toCoordinate(element: JsonElement?): Coordinate? {
        val coord = when (element) {
            is JsonArray -> if (element.size >= 2) {
                val lat = element[0].jsonPrimitive.doubleOrNull
                val lon = element[1].jsonPrimitive.doubleOrNull
                if (lat != null && lon != null) Coordinate(latitude = lat, longitude = lon) else null
            } else null
            is JsonObject -> {
                val lat = (element["latitude"] ?: element["lat"]) as? JsonPrimitive
                val lon = (element["longitude"] ?: element["lon"]) as? JsonPrimitive
                val latitude = lat?.doubleOrNull
                val longitude = lon?.doubleOrNull
                if (latitude != null && longitude != null) Coordinate(latitude, longitude) else null
            }
            else -> null
        } ?: return null
        return validCoordinate(coord)
    }
}
