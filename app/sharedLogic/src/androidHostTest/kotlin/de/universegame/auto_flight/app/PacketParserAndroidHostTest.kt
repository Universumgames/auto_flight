@file:OptIn(ExperimentalStdlibApi::class)

package de.universegame.auto_flight.app

import de.universegame.auto_flight.app.wire.ComponentStatus
import de.universegame.auto_flight.app.wire.DataRequestPacket
import de.universegame.auto_flight.app.wire.FlightPacket
import de.universegame.auto_flight.app.wire.FrontendPacket
import de.universegame.auto_flight.app.wire.FrontendPackets
import de.universegame.auto_flight.app.wire.PacketType
import de.universegame.auto_flight.app.wire.PlannedAreaPacket
import de.universegame.auto_flight.app.wire.PlannedRoutePacket
import de.universegame.auto_flight.app.wire.PositionUpdate
import de.universegame.auto_flight.app.wire.RouteAlgorithm
import de.universegame.auto_flight.app.wire.RouteMetaData
import de.universegame.auto_flight.app.wire.RouteSettings
import de.universegame.auto_flight.app.wire.RouteType
import de.universegame.auto_flight.app.wire.SensorUpdate
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertNull
import kotlin.test.assertTrue

class PacketParserTest {
    private val planeId: PlaneID = 0xABCD1234u

    private fun parse(packet: FrontendPacket, state: AppState = AppState()): AppState {
        val event = PacketParser.parse(FrontendPackets.encode(packet), state)
        return assertIs<WebSocketEvent.StateUpdate>(event).state
    }

    @Test
    fun parsesPongFrame() {
        val event = PacketParser.parse("pong", AppState())
        assertEquals(WebSocketEvent.Pong, event)
    }

    @Test
    fun ignoresMalformedFrame() {
        assertNull(PacketParser.parse("""{"type":"flight"}""", AppState()))
        assertNull(PacketParser.parse("zz", AppState()))
    }

    @Test
    fun ignoresPacketsWithoutStateEffect() {
        val hex = FrontendPackets.encode(DataRequestPacket(id = planeId, requestedType = PacketType.POSITION))
        assertNull(PacketParser.parse(hex, AppState()))
    }

    @Test
    fun parsesBinaryFrame() {
        val bytes = FrontendPackets.encode(PositionUpdate(id = planeId, position = Coordinate(51.4, 6.6), timestamp = 5L)).hexToByteArray()
        val event = assertIs<WebSocketEvent.StateUpdate>(PacketParser.parse(bytes, AppState()))
        assertEquals(Coordinate(51.4, 6.6), event.state.planes.getValue(planeId).position)
    }

    @Test
    fun routesPositionBySourceId() {
        val state = parse(PositionUpdate(id = BASE_ID, position = Coordinate(51.3, 6.5), timestamp = 7L))
        assertEquals(Coordinate(51.3, 6.5), state.basePosition)
        assertEquals(7L, state.basePositionUpdateTime)
        assertTrue(state.planes.isEmpty())

        parse(PositionUpdate(id = planeId, position = Coordinate(51.4, 6.6), timestamp = 8L), state)
        val plane = state.planes.getValue(planeId)
        assertEquals(Coordinate(51.4, 6.6), plane.position)
        assertEquals(8L, plane.positionUpdateTime)
    }

    @Test
    fun keepsLastPositionOnInvalidCoordinate() {
        val state = parse(PositionUpdate(id = planeId, position = Coordinate(51.4, 6.6)))
        parse(PositionUpdate(id = planeId), state) // defaults to COORDINATE_INVALID_VALUE
        assertEquals(Coordinate(51.4, 6.6), state.planes.getValue(planeId).position)
    }

    @Test
    fun appliesPlaneComponentStatus() {
        val state = parse(
            ComponentStatus(
                id = planeId,
                gps = ConnectionState.CONNECTED,
                barometer = ConnectionState.CONNECTING,
                motorControl = ConnectionState.CONNECTED,
                magnetometer = ConnectionState.DISCONNECTED,
                accelerometer = ConnectionState.CONNECTED,
                battery = ConnectionState.CONNECTED,
                manualOverride = true,
                flightState = FlightState.FLYING,
            ),
        )
        val plane = state.planes.getValue(planeId)
        assertEquals(ConnectionState.CONNECTED, plane.connectionState)
        assertEquals(ConnectionState.CONNECTING, plane.barometerConnection)
        assertEquals(ConnectionState.DISCONNECTED, plane.magnetometerConnection)
        assertEquals(ConnectionState.CONNECTED, plane.batteryConnection)
        assertTrue(plane.manualOverride)
        assertEquals(FlightState.FLYING, plane.flightState)
    }

    @Test
    fun appliesSensorUpdate() {
        val state = parse(SensorUpdate(id = BASE_ID, pressure = 1013.25f, altitude = 0f, heading = 0, batteryPercent = 90))
        assertEquals(1013.25, state.pressureBase)
        assertEquals(90, state.batteryPercentageBase)

        parse(SensorUpdate(id = planeId, pressure = 1000.5f, altitude = 120f, heading = 270, batteryPercent = 77), state)
        val plane = state.planes.getValue(planeId)
        assertEquals(1000.5, plane.pressure)
        assertEquals(270.0, plane.heading)
        assertEquals(77, plane.batteryPercentage)
    }

    @Test
    fun appliesAreaRouteAndFlightData() {
        val settings = RouteSettings(RouteAlgorithm.BOUSTROPHEDON, 30)
        val area = listOf(Coordinate(51.1, 6.1), Coordinate(51.2, 6.2), Coordinate(51.3, 6.1))
        val route = listOf(Coordinate(51.1, 6.1), Coordinate(51.2, 6.2))

        val state = parse(PlannedAreaPacket(id = planeId, shape = area, settings = settings))
        parse(
            PlannedRoutePacket(
                id = planeId, route = route, settings = settings,
                metadata = RouteMetaData(settings = settings, routeType = RouteType.ENTIRE), timestamp = 9L,
            ),
            state,
        )
        parse(
            FlightPacket(id = planeId, state = FlightState.FLYING, nextWaypointIndex = 1uL, nextWaypoint = route[1], targetAltitude = 80f),
            state,
        )

        val plane = state.planes.getValue(planeId)
        assertEquals(AreaData(area, settings), plane.area)
        assertEquals(route, plane.plannedRoute)
        assertEquals(9L, plane.plannedRouteUpdateTime)
        assertEquals(route[1], plane.nextRouteWaypoint)
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
