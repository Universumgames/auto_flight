package de.universegame.auto_flight.app

/**
 * [AppState]-consuming half of [ConnectionItems] (see that object's doc comment for
 * why this is split out into androidMain as extension functions rather than members).
 */
fun ConnectionItems.baseStationItem(state: AppState) = ConnectionItem(
    label = "Base Station",
    status = state.connectionStateBaseStation,
    subTasks = listOf(
        SubTask("Connection", stateOf(isConnected(state.connectionStateBaseStation))),
        SubTask(
            "GPS Position" + (state.basePosition?.let {
                " (${formatCoordinate(it.latitude)}, ${formatCoordinate(it.longitude)})"
            } ?: ""),
            stateOf(isConnected(state.gpsConnectionBase)),
        ),
        SubTask(
            "Barometer" + (if (state.pressureBase != 0.0) " (${formatPressure(state.pressureBase)}hPa)" else ""),
            stateOf(isConnected(state.barometerConnectionBase)),
        ),
    ),
)

/** One [ConnectionItem] per connected plane, keyed by [AppState.planes]. */
fun ConnectionItems.planeItems(state: AppState): List<ConnectionItem> {
    val planes = state.planes.values.ifEmpty { listOf(PlaneInfo(id = DEFAULT_PLANE_ID)) }
    return planes.map { plane -> planeItem(plane) }
}

private fun ConnectionItems.planeItem(plane: PlaneInfo) = ConnectionItem(
    label = "Plane (${plane.id})",
    status = plane.connectionState,
    subTasks = listOf(
        SubTask("Connection", stateOf(isConnected(plane.connectionState))),
        SubTask(
            "GPS Position" + (plane.position?.let {
                " (${formatCoordinate(it.latitude)}, ${formatCoordinate(it.longitude)})"
            } ?: ""),
            stateOf(isConnected(plane.gpsConnection)),
        ),
        SubTask("Motor Controller", stateOf(isConnected(plane.motorComConnection))),
        SubTask(
            "Autopilot Control" + (if (plane.manualOverride) " (manual override active)" else ""),
            stateOf(!plane.manualOverride),
        ),
        SubTask(
            "Magnetometer" + (if (isConnected(plane.magnetometerConnection)) " (${plane.heading.toInt()}°)" else ""),
            stateOf(isConnected(plane.magnetometerConnection)),
        ),
        SubTask("Accelerometer", stateOf(isConnected(plane.accelerometerConnection))),
        SubTask(
            "Barometer" + (if (plane.pressure != 0.0) " (${formatPressure(plane.pressure)}hPa)" else ""),
            stateOf(isConnected(plane.barometerConnection)),
        ),
    ),
)

fun ConnectionItems.totalIsConnected(state: AppState): Boolean =
    isConnected(baseStationItem(state).status) && planeItems(state).all { isConnected(it.status) }

/** True when a plane and its motor controller are connected but autopilot control is disabled. */
fun ConnectionItems.autopilotDisabledWarning(state: AppState): Boolean =
    state.planes.values.any { plane ->
        isConnected(plane.connectionState) && isConnected(plane.motorComConnection) && plane.manualOverride
    }

/** True when a plane is connected but its motor controller is not. */
fun ConnectionItems.motorControllerDisconnectedError(state: AppState): Boolean =
    state.planes.values.any { plane -> isConnected(plane.connectionState) && !isConnected(plane.motorComConnection) }

/** True when a plane is connected but no GPS position is available for it. */
fun ConnectionItems.gpsPlaneUnavailableError(state: AppState): Boolean =
    state.planes.values.any { plane -> isConnected(plane.connectionState) && !isConnected(plane.gpsConnection) }

private fun stateOf(done: Boolean) = if (done) SubTaskState.DONE else SubTaskState.LOADING

private fun formatCoordinate(value: Double): String {
    val rounded = kotlin.math.round(value * 1_000_000) / 1_000_000
    return rounded.toString()
}

private fun formatPressure(value: Double): String {
    val rounded = kotlin.math.round(value * 100) / 100
    return rounded.toString()
}
