package de.universegame.auto_flight.app

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class PacketParserTest {

    @Test
    fun parsesPongFrame() {
        val event = PacketParser.parse("pong", AppState())
        assertEquals(WebSocketEvent.Pong, event)
    }

    @Test
    fun ignoresUnknownPacketType() {
        val event = PacketParser.parse("""{"type":"unknown"}""", AppState())
        assertNull(event)
    }

    @Test
    fun appliesConnectionPacketWithArrayCoordinateAndCaseInsensitiveEnums() {
        val raw = """
            {
              "type": "connection",
              "baseConnectionState": "CONNECTED",
              "planeConnectionState": "connecting",
              "manualOverridePlane": true,
              "flightState": "flying"
            }
        """.trimIndent()

        val event = PacketParser.parse(raw, AppState()) as WebSocketEvent.StateUpdate
        val plane = event.state.planes.getValue(DEFAULT_PLANE_ID)
        assertEquals(ConnectionState.CONNECTED, event.state.connectionStateBaseStation)
        assertEquals(ConnectionState.CONNECTING, plane.connectionState)
        assertTrue(plane.manualOverride)
        assertEquals(FlightState.FLYING, plane.flightState)
    }

    @Test
    fun rejectsSentinelZeroCoordinate() {
        val raw = """{"type":"flight","basePosition":[0,0]}"""
        val event = PacketParser.parse(raw, AppState()) as WebSocketEvent.StateUpdate
        assertNull(event.state.basePosition)
    }

    @Test
    fun parsesFlightPacketWithObjectCoordinatesAndRoute() {
        val raw = """
            {
              "type": "flight",
              "basePosition": {"latitude": 51.3, "longitude": 6.5},
              "planePosition": {"lat": 51.4, "lon": 6.6},
              "plannedRoute": [{"latitude": 51.1, "longitude": 6.1}, [51.2, 6.2]]
            }
        """.trimIndent()

        val event = PacketParser.parse(raw, AppState()) as WebSocketEvent.StateUpdate
        val plane = event.state.planes.getValue(DEFAULT_PLANE_ID)
        assertEquals(Coordinate(51.3, 6.5), event.state.basePosition)
        assertEquals(Coordinate(51.4, 6.6), plane.position)
        assertEquals(2, plane.plannedRoute?.size)
    }
}

class ConnectionItemsTest {

    @Test
    fun totalIsConnectedRequiresEverySubsystem() {
        val disconnected = AppState()
        assertEquals(false, ConnectionItems.totalIsConnected(disconnected))

        val connected = AppState(
            connectionStateBaseStation = ConnectionState.CONNECTED,
            gpsConnectionBase = ConnectionState.CONNECTED,
            barometerConnectionBase = ConnectionState.CONNECTED,
        ).apply {
            planes[DEFAULT_PLANE_ID] = PlaneInfo(
                id = DEFAULT_PLANE_ID,
                connectionState = ConnectionState.CONNECTED,
                gpsConnection = ConnectionState.CONNECTED,
                barometerConnection = ConnectionState.CONNECTED,
                motorComConnection = ConnectionState.CONNECTED,
                magnetometerConnection = ConnectionState.CONNECTED,
                accelerometerConnection = ConnectionState.CONNECTED,
            )
        }
        assertTrue(ConnectionItems.totalIsConnected(connected))
    }

    @Test
    fun motorControllerDisconnectedErrorOnlyWhenPlaneConnected() {
        val planeOffline = AppState().apply {
            planes[DEFAULT_PLANE_ID] = PlaneInfo(id = DEFAULT_PLANE_ID, connectionState = ConnectionState.CONNECTING)
        }
        assertEquals(false, ConnectionItems.motorControllerDisconnectedError(planeOffline))

        val planeOnlineNoMotor = AppState().apply {
            planes[DEFAULT_PLANE_ID] = PlaneInfo(id = DEFAULT_PLANE_ID, connectionState = ConnectionState.CONNECTED)
        }
        assertTrue(ConnectionItems.motorControllerDisconnectedError(planeOnlineNoMotor))
    }

    @Test
    fun autopilotDisabledWarningRequiresManualOverride() {
        val state = AppState().apply {
            planes[DEFAULT_PLANE_ID] = PlaneInfo(
                id = DEFAULT_PLANE_ID,
                connectionState = ConnectionState.CONNECTED,
                motorComConnection = ConnectionState.CONNECTED,
                manualOverride = true,
            )
        }
        assertTrue(ConnectionItems.autopilotDisabledWarning(state))
    }
}
