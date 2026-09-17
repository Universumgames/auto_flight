package de.universegame.auto_flight.app

import kotlin.uuid.Uuid

typealias PlaneID = UInt

/** Key used for the single plane reported by the current base-station protocol, which doesn't yet tag packets with a plane id. */
const val DEFAULT_PLANE_ID: PlaneID = 0u
const val BASE_ID: PlaneID = DEFAULT_PLANE_ID

/**
 * Mirrors the connection lifecycle reported by the base station / plane links.
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
}

/**
 * Overall flight lifecycle as reported by the base station.
 */
enum class FlightState {
    PLANNING,
    PLANNED,
    FLYING,
    RETURNING,
}

/**
 * Steps of the on-device flight configuration wizard (connect -> plan -> fly).
 */
enum class ConfigurationState {
    CONNECTING("configurationState.connecting", -1),
    CONNECTION("configurationState.connection", 0),
    AREA_SELECTION("configurationState.area.selection", 1),
    ROUTE_APPROVAL("configurationState.route.approval", 2),
    STARTING("configurationState.starting", 3),
    FLYING("configurationState.flying", 4),
    FINISHING("configurationState.finishing", 5),;

    val translationKey: String
    val index: Int

    constructor(translationKey: String, index: Int) {
        this.translationKey = translationKey
        this.index = index
    }

    val next: ConfigurationState
    get() = fromIndex(index + 1)

    companion object {
        fun fromIndex(index: Int): ConfigurationState {
            return when (index) {
                -1 -> CONNECTION
                0 -> AREA_SELECTION
                1 -> ROUTE_APPROVAL
                2 -> STARTING
                3 -> FLYING
                4 -> FINISHING
                5 -> CONNECTION
                else -> CONNECTION
            }
        }
    }
}

data class Coordinate(
    val latitude: Double,
    val longitude: Double,
){
    companion object{
        val defaultLocation: Coordinate
            get() = Coordinate(51.316310347903176, 6.569530261539499)
    }
}

// PlaneInfo and AppState live in androidMain (sharedLogic/src/androidMain/.../AppState.kt),
// not here, even though every other model in this file is shared. Kotlin's Swift Export
// (still experimental) crashes while generating collection-bridging code for a
// `MutableMap<PlaneID, PlaneInfo>`-typed property, so AppState.planes can't be part of
// the commonMain surface it exports. That's fine: only Android/JVM ever constructs an
// AppState - the iOS app has its own native `AppState` (`Models/AppState.swift`).
