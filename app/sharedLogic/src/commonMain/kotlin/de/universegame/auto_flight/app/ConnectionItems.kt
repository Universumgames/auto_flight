package de.universegame.auto_flight.app

import de.universegame.auto_flight.app.wire.ConnectionState

enum class SubTaskState {
    DONE, LOADING;

    companion object {
        fun byBool(bool: Boolean) = if (bool) DONE else LOADING
    }
}

data class SubTask(
    val label: String,
    val state: SubTaskState,
)

enum class ConnectionItemType{
    BASE_STATION,
    PLANE
}

data class ConnectionItem(
    val label: String,
    val connectionItemType: ConnectionItemType,
    val status: ConnectionState,
    val batteryPercent: Int,
    val subTasks: List<SubTask>,
)

/**
 * Derives the connection-overview presentation (grouped links + sub-tasks + the
 * global warning flags shown in the app chrome) from an [AppState] snapshot.
 *
 * Only [isConnected]/[formatStatus] live here in commonMain, since they're the only
 * members the iOS side calls directly (`ConnectionItems.shared.isConnected`/
 * `.formatStatus` from `Models/AppState.swift`); everything that derives from
 * [AppState]/`PlaneInfo` is added as an androidMain extension
 * (`androidMain/.../ConnectionItems.kt`) for the same reason those types live there -
 * see `Models.kt`'s package-level note.
 */
object ConnectionItems {

    fun isConnected(status: ConnectionState) = status == ConnectionState.CONNECTED

    fun formatStatus(status: ConnectionState): String =
        status.name.lowercase().replaceFirstChar { it.uppercase() }
}
