import Foundation
import SharedLogic

/// Key used for the single plane reported by the current base-station protocol,
/// which doesn't yet tag packets with a plane id. Mirrors the Kotlin side's
/// `DEFAULT_PLANE_ID` (`sharedLogic/.../Models.kt`).
let defaultPlaneID: PlaneID = "default"

enum PacketParsing {

    /// Returns the plane's existing entry in `current.planes`, creating and inserting one if absent.
    static func plane(_ current: AppState, id: PlaneID = defaultPlaneID) -> PlaneInfo {
        if let existing = current.planes[id] { return existing }
        let created = PlaneInfo(id: id)
        current.planes[id] = created
        return created
    }

    /// Strips a leading BOM and any NUL characters, then trims whitespace.
    static func normalize(_ raw: String) -> String {
        var s = raw
        if s.hasPrefix("\u{FEFF}") { s.removeFirst() }
        s = s.replacingOccurrences(of: "\u{0000}", with: "")
        return s.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    /// Returns nil for the (0,0) sentinel and coordinates below -200.
    private static func toCoordinate(_ coord: Coordinate) -> Coordinate? {
        if coord.latitude == 0 && coord.longitude == 0 { return nil }
        if coord.latitude < -200 || coord.longitude < -200 { return nil }
        return coord
    }

    private static func toConnectionState(_ state: wire.ConnectionState) -> ConnectionState {
        state == .CONNECTED ? .CONNECTED : .CONNECTING
    }

    private static func toFlightState(_ state: wire.FlightState) -> FlightState {
        switch state {
        case .PLANNED: return .PLANNED
        case .FLYING: return .FLYING
        case .RETURNING: return .RETURNING
        default: return .PLANNING
        }
    }

    static func applyFlightPacket(_ current: AppState, _ packet: wire.FlightUpdatePacket) {
        current.basePosition = toCoordinate(packet.basePosition) ?? current.basePosition
        current.basePositionUpdateTime = packet.basePositionUpdateTime

        let plane = self.plane(current)
        plane.position = toCoordinate(packet.planePosition) ?? plane.position
        plane.positionUpdateTime = packet.planePositionUpdateTime
        plane.flightRoute = packet.flightRoute.isEmpty ? nil : packet.flightRoute
        plane.flightRouteUpdateTime = packet.flightRouteUpdateTime
        plane.plannedRoute = packet.plannedRoute.isEmpty ? nil : packet.plannedRoute
        plane.plannedRouteUpdateTime = packet.plannedRouteUpdateTime
    }

    static func applyConnectionPacket(_ current: AppState, _ packet: wire.ConnectionUpdatePacket) {
        //current.connectionStateBaseStation = toConnectionState(packet.baseConnectionState)
        current.lastContactBaseStationTimestamp = packet.lastContactBaseStationTimestamp
        current.gpsConnectionBase = toConnectionState(packet.gpsConnectionBase)
        current.barometerConnectionBase = toConnectionState(packet.barometerConnectionBase)

        let plane = self.plane(current)
        plane.connectionState = toConnectionState(packet.planeConnectionState)
        plane.lastContactTimestamp = packet.lastContactPlaneTimestamp
        plane.gpsConnection = toConnectionState(packet.gpsConnectionPlane)
        plane.barometerConnection = toConnectionState(packet.barometerConnectionPlane)
        plane.motorComConnection = toConnectionState(packet.motorComConnectionPlane)
        plane.magnetometerConnection = toConnectionState(packet.magnetometerConnectionPlane)
        plane.accelerometerConnection = toConnectionState(packet.accelerometerConnectionPlane)
        plane.manualOverride = packet.manualOverridePlane
        plane.flightState = toFlightState(packet.flightState)
    }

    static func applySensorPacket(_ current: AppState, _ packet: wire.SensorPacket) {
        if packet.barometerPressureBase != 0 { current.pressureBase = Double(packet.barometerPressureBase) }

        let plane = self.plane(current)
        if packet.barometerPressurePlane != 0 { plane.pressure = Double(packet.barometerPressurePlane) }
        if packet.calculatedAltitude != 0 { plane.calculatedAltitude = Double(packet.calculatedAltitude) }
        plane.heading = Double(packet.headingPlane)
    }

    static func applyBatteryStatusPacket(_ current: AppState, _ packet: wire.BatteryStatusPacket) {
        current.batteryPercentageBase = Int(packet.baseBatteryPercentage)

        let plane = self.plane(current)
        plane.batteryPercentage = Int(packet.planeBatteryPercentage)
    }
}
