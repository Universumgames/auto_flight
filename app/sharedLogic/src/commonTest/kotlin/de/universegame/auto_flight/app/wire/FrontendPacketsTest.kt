package de.universegame.auto_flight.app.wire

import de.universegame.auto_flight.app.Coordinate
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/**
 * Wire compatibility with the firmware: the hex fixtures are CBOR exactly as the C++ side
 * (FlightPackets.generated.hpp, nlohmann::json::to_cbor) serializes these packets, all sent
 * with device id 0xABCD1234.
 */
class FrontendPacketsTest {
    /** SensorUpdate(1700000000, pressure = 1013.25, altitude = 120.5, heading = 270, batteryPercent = 77) */
    private val sensorUpdateHex = "a768616c746974756465fa42f100006e6261747465727950657263656e74184d6768656164696e6719010e6269641aabcd1234687072657373757265fa447d50006974696d657374616d701a6553f100647479706510"

    /** PlannedRoutePacket(1700000001, [(6.5, 51.25), (6.75, 51.5)], {BOUSTROPHEDON, 30},
     *  metadata = {{BOUSTROPHEDON, 30}, hash = 0xDEADBEEF, ENTIRE, entireSize = 2, length = 1234.5, estimatedTime = 60}) */
    private val plannedRouteHex = "a66269641aabcd1234686d65746164617461a66a656e7469726553697a65026d657374696d6174656454696d65183c64686173681adeadbeef666c656e677468fa449a500069726f7574655479706566656e746972656873657474696e6773a2716f7665726c617050657263656e74616765181e6e726f757465416c676f726974686d6d626f757374726f706865646f6e65726f75746582a2686c61746974756465fa424d0000696c6f6e676974756465fa40d00000a2686c61746974756465fa424e0000696c6f6e676974756465fa40d800006873657474696e6773a2716f7665726c617050657263656e74616765181e6e726f757465416c676f726974686d6d626f757374726f706865646f6e6974696d657374616d701a6553f10164747970651830"

    /** FlightPacket(1700000002, FLYING, nextWaypointIndex = 3, (6.5, 51.25), targetAltitude = 80,
     *  lastCheckedArea = [(6.5, 51.25), (6.75, 51.25), (6.75, 51.5), (6.5, 51.5)]) */
    private val flightPacketHex = "a86269641aabcd12346f6c617374436865636b656441726561a16a61726561506f696e747384a2686c61746974756465fa424d0000696c6f6e676974756465fa40d00000a2686c61746974756465fa424d0000696c6f6e676974756465fa40d80000a2686c61746974756465fa424e0000696c6f6e676974756465fa40d80000a2686c61746974756465fa424e0000696c6f6e676974756465fa40d000006c6e657874576179706f696e74a2686c61746974756465fa424d0000696c6f6e676974756465fa40d00000716e657874576179706f696e74496e6465780365737461746566666c79696e676e746172676574416c746974756465fa42a000006974696d657374616d701a6553f10264747970651840"

    /** DataRequest(1700000003, requestedType = ROUTE_HISTORY) */
    private val dataRequestHex = "a46269641aabcd12346d7265717565737465645479706518326974696d657374616d701a6553f103647479706518ff"

    @Test
    fun decodesSensorUpdate() {
        assertEquals(
            SensorUpdate(id = 0xABCD1234u, pressure = 1013.25f, altitude = 120.5f, heading = 270, batteryPercent = 77, timestamp = 1700000000L),
            FrontendPackets.decode(sensorUpdateHex),
        )
    }

    @Test
    fun decodesPlannedRoute() {
        val packet = assertNotNull(FrontendPackets.decodePlannedRoute(plannedRouteHex))
        assertEquals(listOf(Coordinate(51.25, 6.5), Coordinate(51.5, 6.75)), packet.route)
        assertEquals(RouteSettings(RouteAlgorithm.BOUSTROPHEDON, 30), packet.settings)
        assertEquals(0xDEADBEEFuL, packet.metadata.hash)
    }

    @Test
    fun decodesFlightPacket() {
        val packet = assertNotNull(FrontendPackets.decodeFlightPacket(flightPacketHex))
        assertEquals(FlightState.FLYING, packet.state)
        assertEquals(3uL, packet.nextWaypointIndex)
        assertEquals(Coordinate(51.25, 6.5), packet.nextWaypoint)
        assertEquals(80f, packet.targetAltitude)
        assertEquals(4, packet.lastCheckedArea.areaPoints.size)
    }

    @Test
    fun decodesDataRequest() {
        val packet = FrontendPackets.decode(dataRequestHex)
        assertEquals(DataRequestPacket(id = 0xABCD1234u, requestedType = PacketType.ROUTE_HISTORY, timestamp = 1700000003L), packet)
    }

    @Test
    fun typedDecoderRejectsOtherPacketType() {
        assertNull(FrontendPackets.decodeFlightPacket(sensorUpdateHex))
    }

    @Test
    fun roundTripsEveryField() {
        val packet = ComponentStatus(
            id = 42u,
            gps = ConnectionState.CONNECTED,
            barometer = ConnectionState.CONNECTING,
            motorControl = ConnectionState.CONNECTED,
            magnetometer = ConnectionState.CONNECTED,
            accelerometer = ConnectionState.CONNECTING,
            battery = ConnectionState.CONNECTED,
            manualOverride = true,
            flightState = FlightState.PLANNED,
            timestamp = 1700000009L,
        )
        assertEquals(packet, FrontendPackets.decode(FrontendPackets.encode(packet)))
    }

    @Test
    fun encodesFieldsEqualToTheirDefault() {
        // The C++ side requires every key, so a default-valued field (hash = 0) must still be sent.
        val packet = PlannedRoutePacket(id = 1u, route = emptyList(), settings = RouteSettings(RouteAlgorithm.BASIC),
            metadata = RouteMetaData(settings = RouteSettings(RouteAlgorithm.BASIC), hash = 0uL, routeType = RouteType.ENTIRE), timestamp = 1L)
        assertEquals(packet, FrontendPackets.decode(FrontendPackets.encode(packet)))
        assertEquals(true, FrontendPackets.encode(packet).contains("68617368")) // "hash" key
    }
}
