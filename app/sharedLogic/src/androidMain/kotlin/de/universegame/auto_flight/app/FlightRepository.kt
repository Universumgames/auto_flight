package de.universegame.auto_flight.app

/** Receives live [AppState] snapshots from a [FlightRepository]. */
fun interface StateListener {
    fun onStateChanged(state: AppState)
}

/**
 * Everything the UI needs to talk to a base station: a live [AppState] stream plus
 * the handful of one-shot REST calls used while planning a flight.
 *
 * This is intentionally an interface so the transport can be swapped later (a mock
 * for previews/tests, a different protocol, a local simulator, ...) without any UI
 * code needing to change. [KtorFlightRepository] is the production implementation,
 * talking to the base station over a websocket + plain HTTP, as the previous web
 * frontend did.
 */
interface FlightRepository {
    /** Most recently known state; equivalent to the last value delivered to listeners. */
    fun currentState(): AppState

    fun addListener(listener: StateListener)
    fun removeListener(listener: StateListener)

    /** Opens the live connection to `host` (e.g. "192.168.4.1" or "192.168.4.1:8080"). */
    fun connect(host: String)

    /** Closes the live connection, if any. */
    fun disconnect()

    /**
     * Fetches the currently configured survey area polygon (`null` if none is set) and
     * delivers it to [onResult] once the request completes.
     *
     * Callback-based rather than `suspend` because Kotlin's Swift Export (still
     * experimental) currently fails to bridge `suspend fun`s: it unconditionally routes
     * them through kotlinx-coroutines-core's `KotlinCoroutineSupport` shim, which does
     * not compile against the kotlinx-coroutines-core version pinned in this project.
     * Plain function-type parameters export to Swift closures without that dependency.
     */
    fun fetchArea(onResult: (List<Coordinate>?) -> Unit)

    /** Submits a new survey area polygon for the base station to plan a route over. */
    fun submitArea(polygon: List<Coordinate>, onResult: (AreaSubmitResult) -> Unit)

    /** Fetches the route planned by the base station for the current survey area. */
    fun fetchRoute(onResult: (List<Coordinate>) -> Unit)
}
