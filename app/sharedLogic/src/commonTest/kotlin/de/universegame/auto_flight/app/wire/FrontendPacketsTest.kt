package de.universegame.auto_flight.app.wire

import de.universegame.auto_flight.app.Coordinate
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

class FrontendPacketsTest {

    @Test
    fun flightUpdatePacketRoundTrips() {
        val original = FlightUpdatePacket(
            basePosition = Coordinate(51.316310347903176, 6.569530261539499),
            basePositionUpdateTime = 1_000L,
            planePosition = Coordinate(51.4, 6.6),
            planePositionUpdateTime = 2_000L,
            flightRoute = listOf(Coordinate(1.0, 2.0), Coordinate(3.0, 4.0)),
            flightRouteUpdateTime = 3_000L,
            plannedRoute = emptyList(),
            plannedRouteUpdateTime = 4_000L,
        )

        val hex = FrontendPackets.encode(original)
        val decoded = FrontendPackets.decodeFlightUpdate(hex)

        assertEquals(original, assertNotNull(decoded))
    }

    @Test
    fun connectionUpdatePacketRoundTripsEnums() {
        val original = ConnectionUpdatePacket(
            baseConnectionState = ConnectionState.CONNECTED,
            lastContactBaseStationTimestamp = 1L,
            planeConnectionState = ConnectionState.CONNECTING,
            lastContactPlaneTimestamp = 2L,
            gpsConnectionBase = ConnectionState.CONNECTED,
            gpsConnectionPlane = ConnectionState.CONNECTED,
            barometerConnectionBase = ConnectionState.CONNECTING,
            barometerConnectionPlane = ConnectionState.CONNECTING,
            motorComConnectionPlane = ConnectionState.CONNECTED,
            magnetometerConnectionPlane = ConnectionState.CONNECTED,
            accelerometerConnectionPlane = ConnectionState.CONNECTED,
            manualOverridePlane = true,
            flightState = FlightState.FLYING,
        )

        val hex = FrontendPackets.encode(original)
        val decoded = FrontendPackets.decodeConnectionUpdate(hex)

        assertEquals(original, assertNotNull(decoded))
    }

    @Test
    fun areaDefinePacketHasNoTypeTagOnWire() {
        val original = AreaDefinePacket(shape = listOf(Coordinate(1.0, 1.0), Coordinate(2.0, 2.0)))

        val hex = FrontendPackets.encode(original)
        val decoded = FrontendPackets.decodeAreaDefine(hex)

        assertEquals(original, assertNotNull(decoded))
    }

    @Test
    fun malformedHexDecodesToNull() {
        assertEquals(null, FrontendPackets.decodeSensor("not-hex"))
    }
}
