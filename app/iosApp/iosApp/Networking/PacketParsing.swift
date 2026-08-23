import Foundation
import SharedLogic

/// Key used for the single plane reported by the current base-station protocol,
/// which doesn't yet tag packets with a plane id. Mirrors the Kotlin side's
/// `DEFAULT_PLANE_ID` (`sharedLogic/.../Models.kt`).
let defaultPlaneID: PlaneID = "default"

/// Native Swift re-implementation of the parsing rules in
/// `sharedLogic/src/commonMain/kotlin/.../PacketParser.kt`. Kept as free functions
/// operating on Foundation JSON (`[String: Any]`) rather than reusing the shared
/// Kotlin `PacketParser`, because its methods take `kotlinx.serialization.json`
/// types that aren't practical to construct from Swift.
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

    static func toDouble(_ value: Any?) -> Double? {
        switch value {
        case let n as NSNumber: return n.doubleValue
        case let s as String: return Double(s)
        default: return nil
        }
    }

    /// Accepts a `[lat, lon]` array or an object with `lat`/`lon` or
    /// `latitude`/`longitude` keys. Rejects the `(0,0)` sentinel and anything with
    /// a coordinate below -200.
    static func toCoordinate(_ value: Any?) -> Coordinate? {
        guard let value else { return nil }
        var coord: Coordinate?

        if let arr = value as? [Any], arr.count >= 2,
           let lat = toDouble(arr[0]), let lon = toDouble(arr[1]) {
            coord = Coordinate(latitude: lat, longitude: lon)
        }

        if let obj = value as? [String: Any] {
            if let lat = toDouble(obj["lat"]), let lon = toDouble(obj["lon"]) {
                coord = Coordinate(latitude: lat, longitude: lon)
            }
            if let lat = toDouble(obj["latitude"]), let lon = toDouble(obj["longitude"]) {
                coord = Coordinate(latitude: lat, longitude: lon)
            }
        }

        if let c = coord {
            if c.latitude == 0 && c.longitude == 0 { coord = nil }
            else if c.latitude < -200 || c.longitude < -200 { coord = nil }
        }

        return coord
    }

    static func toRoute(_ value: Any?) -> [Coordinate]? {
        guard let arr = value as? [Any] else { return nil }
        return arr.compactMap { toCoordinate($0) }
    }

    /// A number is epoch millis if `> 1e12`, else epoch seconds. A string is
    /// parsed as ISO-8601.
    static func toTimeT(_ value: Any?) -> Int64? {
        guard let value else { return nil }
        if let n = value as? NSNumber {
            let d = n.doubleValue
            return Int64(d > 1e12 ? (d / 1000).rounded(.down) : d.rounded(.down))
        }
        if let s = value as? String {
            let withFractional = ISO8601DateFormatter()
            withFractional.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
            if let date = withFractional.date(from: s) {
                return Int64(date.timeIntervalSince1970)
            }
            let plain = ISO8601DateFormatter()
            if let date = plain.date(from: s) {
                return Int64(date.timeIntervalSince1970)
            }
            return nil
        }
        return nil
    }

    static func parseConnectionState(_ value: Any?) -> ConnectionState? {
        guard let s = value as? String else { return nil }
        switch s.uppercased() {
        case "CONNECTING": return .CONNECTING
        case "CONNECTED": return .CONNECTED
        default: return nil
        }
    }

    static func parseFlightState(_ value: Any?) -> FlightState? {
        guard let s = value as? String else { return nil }
        switch s.uppercased() {
        case "PLANNING": return .PLANNING
        case "PLANNED": return .PLANNED
        case "FLYING": return .FLYING
        case "RETURNING": return .RETURNING
        default: return nil
        }
    }

    static func toBool(_ value: Any?) -> Bool? {
        if let n = value as? NSNumber, CFGetTypeID(n) == CFBooleanGetTypeID() { return n.boolValue }
        if let b = value as? Bool { return b }
        return nil
    }

    static func applyFlightPacket(_ current: AppState, _ packet: [String: Any]) {
        let basePos = toCoordinate(packet["basePosition"])
        let planePos = toCoordinate(packet["planePosition"])

        current.basePosition = basePos ?? current.basePosition
        current.basePositionUpdateTime = toTimeT(packet["basePositionUpdateTime"])

        let plane = self.plane(current)
        plane.position = planePos ?? plane.position
        plane.positionUpdateTime = toTimeT(packet["planePositionUpdateTime"])
        plane.flightRoute = toRoute(packet["flightRoute"])
        plane.flightRouteUpdateTime = toTimeT(packet["flightRouteUpdateTime"])
        plane.plannedRoute = toRoute(packet["plannedRoute"])
        plane.plannedRouteUpdateTime = toTimeT(packet["plannedRouteUpdateTime"])
    }

    static func applyConnectionPacket(_ current: AppState, _ packet: [String: Any]) {
        current.connectionStateBaseStation = parseConnectionState(packet["baseConnectionState"]) ?? current.connectionStateBaseStation
        current.lastContactBaseStationTimestamp = toTimeT(packet["lastContactBaseStationTimestamp"])
        current.gpsConnectionBase = parseConnectionState(packet["gpsConnectionBase"]) ?? current.gpsConnectionBase
        current.barometerConnectionBase = parseConnectionState(packet["barometerConnectionBase"]) ?? current.barometerConnectionBase

        let plane = self.plane(current)
        plane.connectionState = parseConnectionState(packet["planeConnectionState"]) ?? plane.connectionState
        plane.lastContactTimestamp = toTimeT(packet["lastContactPlaneTimestamp"])
        plane.gpsConnection = parseConnectionState(packet["gpsConnectionPlane"]) ?? plane.gpsConnection
        plane.barometerConnection = parseConnectionState(packet["barometerConnectionPlane"]) ?? plane.barometerConnection
        plane.motorComConnection = parseConnectionState(packet["motorComConnectionPlane"]) ?? plane.motorComConnection
        plane.magnetometerConnection = parseConnectionState(packet["magnetometerConnectionPlane"]) ?? plane.magnetometerConnection
        plane.accelerometerConnection = parseConnectionState(packet["accelerometerConnectionPlane"]) ?? plane.accelerometerConnection
        plane.manualOverride = toBool(packet["manualOverridePlane"]) ?? plane.manualOverride
        plane.flightState = parseFlightState(packet["flightState"]) ?? plane.flightState
    }

    static func applySensorPacket(_ current: AppState, _ packet: [String: Any]) {
        let pressureBase = toDouble(packet["barometerPressureBase"])
        let pressurePlane = toDouble(packet["barometerPressurePlane"])
        let calculatedAltitude = toDouble(packet["calculatedAltitude"])
        let headingPlane = toDouble(packet["headingPlane"])

        current.pressureBase = (pressureBase.map { $0 != 0 } == true) ? pressureBase! : current.pressureBase

        let plane = self.plane(current)
        plane.pressure = (pressurePlane.map { $0 != 0 } == true) ? pressurePlane! : plane.pressure
        plane.calculatedAltitude = (calculatedAltitude.map { $0 != 0 } == true) ? calculatedAltitude! : plane.calculatedAltitude
        plane.heading = headingPlane ?? plane.heading
    }
}
