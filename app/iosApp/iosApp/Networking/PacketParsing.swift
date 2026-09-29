import Foundation
import SharedLogic

/// Key used for the single plane reported by the current base-station protocol,
/// which doesn't yet tag packets with a plane id. Mirrors the Kotlin side's
/// `DEFAULT_PLANE_ID` (`sharedLogic/.../Models.kt`).
let defaultPlaneID: PlaneID = SharedLogic.DEFAULT_PLANE_ID

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

    static func applyPositionPacket(
        _ packet: wire.PositionUpdatePacket
    ) {
        if packet.sourceId == SharedLogic.BASE_ID {
            AppState.shared.basePosition = packet.position
            AppState.shared.basePositionUpdateTime = packet.positionUpdateTime
        } else {
            let plane = self.plane(AppState.shared, id: packet.sourceId)
            plane.position = packet.position
            plane.positionUpdateTime = packet.positionUpdateTime
        }
    }

    static func applyConnectionPacket(_ current: AppState, _ packet: wire.ConnectionUpdatePacket) {
        if packet.sourceId == SharedLogic.BASE_ID{
            current.gpsConnectionBase = toConnectionState(packet.gpsConnection)
            current.barometerConnectionBase = toConnectionState(packet.barometer)
        }else {
            let plane = self.plane(current, id: packet.sourceId)
            plane.connectionState = .CONNECTED
            plane.gpsConnection = toConnectionState(packet.gpsConnection)
            plane.barometerConnection = toConnectionState(packet.barometer)
            plane.motorComConnection = toConnectionState(packet.motorCom)
            plane.accelerometerConnection = toConnectionState(packet.accelerometer)
            plane.manualOverride = packet.manualOverride
            plane.flightState = toFlightState(packet.flightState)
        }
    }

    static func applySensorPacket(_ current: AppState, _ packet: wire.SensorPacket) {
        if packet.sourceId == SharedLogic.BASE_ID {
            current.pressureBase = Double(packet.barometerPressure)
        }else{
            let plane = self.plane(current, id: packet.sourceId)
            plane.pressure = Double(packet.barometerPressure)
            plane.calculatedAltitude = Double(packet.calculatedAltitude)
            plane.heading = Double(packet.headingPlane)
        }
    }

    static func applyBatteryStatusPacket(_ current: AppState, _ packet: wire.BatteryStatusPacket) {
        if packet.sourceId == SharedLogic.BASE_ID{
            current.batteryPercentageBase = Int(packet.batteryPercentage)
        }else{
            let plane = self.plane(current, id: packet.sourceId)
            plane.batteryPercentage = Int(packet.batteryPercentage)
        }
    }
    
    static func applyRoutePlannedPacket(_ current: AppState, _ packet: wire.PlannedRoutePacket){
        if packet.sourceId == SharedLogic.BASE_ID {
            return
        }
        let plane = self.plane(current, id: packet.sourceId)
        plane.plannedRoute = packet.route
        plane.plannedRouteUpdateTime = -1 // TODO: update packet to include timestamp
    }
    
    static func applyAreaDefinePacket(
        _ current: AppState,
        _ packet: wire.AreaDefinePacket
    ){
        if packet.sourceId == SharedLogic.BASE_ID{
            return
        }
        let plane = self.plane(current, id: packet.sourceId)
        plane.area = AreaData(
            areaPoints: packet.shape,
            settings: packet.settings
        )
        // TODO: implement area define, add to storage as well
    }
}
