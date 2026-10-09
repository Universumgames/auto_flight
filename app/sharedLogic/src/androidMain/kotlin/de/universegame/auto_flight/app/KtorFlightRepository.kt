package de.universegame.auto_flight.app

import de.universegame.auto_flight.app.wire.ConnectionState
import io.ktor.client.HttpClient
import io.ktor.client.plugins.websocket.DefaultClientWebSocketSession
import io.ktor.client.plugins.websocket.WebSockets
import io.ktor.client.plugins.websocket.webSocket
import io.ktor.client.request.get
import io.ktor.client.request.post
import io.ktor.client.request.setBody
import io.ktor.client.statement.bodyAsText
import io.ktor.http.ContentType
import io.ktor.http.contentType
import io.ktor.http.isSuccess
import io.ktor.websocket.CloseReason
import io.ktor.websocket.Frame
import io.ktor.websocket.close
import io.ktor.websocket.readText
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlin.time.Clock
import kotlin.time.ExperimentalTime
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import kotlin.concurrent.Volatile

private const val PING_INTERVAL_MS = 2_000L
private const val PONG_TIMEOUT_MS = 5_000L
private const val RECONNECT_DELAY_MS = 2_000L

/**
 * Default [FlightRepository]: a websocket at `ws://<host>/api/ws` for live state
 * (mirroring the previous web frontend's `useWebSocket` + heartbeat setup) and
 * plain HTTP calls to `/api/area` and `/api/route` for flight planning.
 */
class KtorFlightRepository(
    private val client: HttpClient = HttpClient { install(WebSockets) },
) : FlightRepository {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    @Volatile
    private var state: AppState = AppState()

    @Volatile
    private var listeners: List<StateListener> = emptyList()

    @Volatile
    private var lastPongReceivedAtMs: Long = 0L

    @Volatile
    private var host: String? = null

    private var connectionJob: Job? = null

    override fun currentState(): AppState = state

    override fun addListener(listener: StateListener) {
        listeners = listeners + listener
        listener.onStateChanged(state)
    }

    override fun removeListener(listener: StateListener) {
        listeners = listeners - listener
    }

    override fun connect(host: String) {
        this.host = host
        disconnect()
        connectionJob = scope.launch {
            while (isActive) {
                try {
                    client.webSocket(urlString = "ws://$host/api/ws") {
                        lastPongReceivedAtMs = nowMs()
                        state.connectionStateBaseStation = ConnectionState.CONNECTED
                        setState(state)

                        val heartbeat = launch { runHeartbeat(this@webSocket) }
                        try {
                            for (frame in incoming) {
                                if (frame is Frame.Text) handleFrame(frame.readText())
                            }
                        } finally {
                            heartbeat.cancel()
                        }
                    }
                } catch (e: CancellationException) {
                    throw e
                } catch (_: Exception) {
                    // connection dropped or never established - fall through to reconnect
                }

                state.connectionStateBaseStation = ConnectionState.CONNECTING
                state.planes.values.forEach { it.connectionState = ConnectionState.CONNECTING }
                setState(state)
                if (isActive) delay(RECONNECT_DELAY_MS)
            }
        }
    }

    override fun disconnect() {
        connectionJob?.cancel()
        connectionJob = null
    }

    override fun fetchArea(onResult: (List<Coordinate>?) -> Unit) {
        scope.launch {
            val result = try {
                val response = client.get("http://${requireHost()}/api/area")
                if (!response.status.isSuccess()) {
                    null
                } else {
                    val obj = appJson.parseToJsonElement(response.bodyAsText()) as? JsonObject
                    val shape = obj?.get("shape") as? JsonArray
                    shape?.mapNotNull { PacketParser.toCoordinate(it) }
                }
            } catch (e: CancellationException) {
                throw e
            } catch (_: Exception) {
                null
            }
            onResult(result)
        }
    }

    override fun submitArea(polygon: List<Coordinate>, onResult: (AreaSubmitResult) -> Unit) {
        scope.launch {
            val result = try {
                val body = buildJsonObject {
                    put(
                        "shape",
                        buildJsonArray {
                            polygon.forEach { coordinate ->
                                add(
                                    buildJsonObject {
                                        put("latitude", coordinate.latitude)
                                        put("longitude", coordinate.longitude)
                                    },
                                )
                            }
                        },
                    )
                }

                val response = client.post("http://${requireHost()}/api/area") {
                    contentType(ContentType.Application.Json)
                    setBody(body.toString())
                }
                if (!response.status.isSuccess()) {
                    AreaSubmitResult.FAILED
                } else {
                    val count = response.bodyAsText().trim().toIntOrNull()
                    if (count == polygon.size) AreaSubmitResult.ACCEPTED else AreaSubmitResult.MISMATCH
                }
            } catch (e: CancellationException) {
                throw e
            } catch (_: Exception) {
                AreaSubmitResult.FAILED
            }
            onResult(result)
        }
    }

    override fun fetchRoute(onResult: (List<Coordinate>) -> Unit) {
        scope.launch {
            val result = try {
                val response = client.get("http://${requireHost()}/api/route")
                if (!response.status.isSuccess()) {
                    emptyList()
                } else {
                    val obj = appJson.parseToJsonElement(response.bodyAsText()) as? JsonObject
                    val route = obj?.get("route") as? JsonArray
                    route?.mapNotNull { PacketParser.toCoordinate(it) } ?: emptyList()
                }
            } catch (e: CancellationException) {
                throw e
            } catch (_: Exception) {
                emptyList()
            }
            onResult(result)
        }
    }

    private fun requireHost(): String =
        checkNotNull(host) { "connect(host) must be called before making requests" }

    private suspend fun runHeartbeat(session: DefaultClientWebSocketSession) {
        while (session.isActive) {
            delay(PING_INTERVAL_MS)
            session.send(Frame.Text("ping"))
            if (nowMs() - lastPongReceivedAtMs > PONG_TIMEOUT_MS) {
                session.close(CloseReason(CloseReason.Codes.NORMAL, "heartbeat timeout"))
                return
            }
        }
    }

    private fun handleFrame(raw: String) {
        when (val event = PacketParser.parse(raw, state)) {
            is WebSocketEvent.Pong -> {
                lastPongReceivedAtMs = nowMs()
                state.connectionStateBaseStation = ConnectionState.CONNECTED
                setState(state)
            }
            is WebSocketEvent.StateUpdate -> setState(event.state)
            null -> Unit
        }
    }

    private fun setState(newState: AppState) {
        state = newState
        listeners.forEach { it.onStateChanged(newState) }
    }

    @OptIn(ExperimentalTime::class)
    private fun nowMs(): Long = Clock.System.now().toEpochMilliseconds()
}
