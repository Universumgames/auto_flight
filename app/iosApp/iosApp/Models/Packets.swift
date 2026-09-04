//
//  Packets.swift
//  iosApp
//
//  Created by Tom Arlt on 29.08.26.
//

import Foundation
import SharedLogic

// MARK: - Coordinate

extension Coordinate: @retroactive Decodable {}
extension Coordinate: @retroactive Encodable {}
extension Coordinate {
    private enum CodingKeys: String, CodingKey {
        case latitude
        case longitude
    }

    public convenience init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let latitude = try container.decode(Double.self, forKey: .latitude)
        let longitude = try container.decode(Double.self, forKey: .longitude)
        self.init(latitude: latitude, longitude: longitude)
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(latitude, forKey: .latitude)
        try container.encode(longitude, forKey: .longitude)
    }
}

// MARK: - ConnectionState / FlightState
//
// Wire values are the NLOHMANN_JSON_SERIALIZE_ENUM strings from
// shared_components/flight_data/types.hpp, not the Kotlin enum case names CoreBluetooth/
// Swift Export exposes (`CONNECTING`, `PLANNING`, ...).

extension wire.ConnectionState: @retroactive Decodable {}
extension wire.ConnectionState: @retroactive Encodable {}
extension wire.ConnectionState {
    public init(from decoder: any Decoder) throws {
        let container = try decoder.singleValueContainer()
        switch try container.decode(String.self) {
        case "connecting": self = .CONNECTING
        case "connected": self = .CONNECTED
        case let other:
            throw DecodingError.dataCorruptedError(in: container, debugDescription: "Unknown ConnectionState '\(other)'")
        }
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.singleValueContainer()
        switch self {
        case .CONNECTING: try container.encode("connecting")
        case .CONNECTED: try container.encode("connected")
        @unknown default:
            throw EncodingError.invalidValue(self, EncodingError.Context(codingPath: encoder.codingPath, debugDescription: "Unknown ConnectionState"))
        }
    }
}

extension wire.FlightState: @retroactive Decodable {}
extension wire.FlightState: @retroactive Encodable {}
extension wire.FlightState {
    public init(from decoder: any Decoder) throws {
        let container = try decoder.singleValueContainer()
        switch try container.decode(String.self) {
        case "planning": self = .PLANNING
        case "planned": self = .PLANNED
        case "flying": self = .FLYING
        case "returning": self = .RETURNING
        case let other:
            throw DecodingError.dataCorruptedError(in: container, debugDescription: "Unknown FlightState '\(other)'")
        }
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.singleValueContainer()
        switch self {
        case .PLANNING: try container.encode("planning")
        case .PLANNED: try container.encode("planned")
        case .FLYING: try container.encode("flying")
        case .RETURNING: try container.encode("returning")
        @unknown default:
            throw EncodingError.invalidValue(self, EncodingError.Context(codingPath: encoder.codingPath, debugDescription: "Unknown FlightState"))
        }
    }
}

// MARK: - Frontend packets
//
// Mirrors base_station/components/frontend/FrontendPackets.hpp. Property names already match
// the wire JSON keys 1:1 (see sharedLogic's de.universegame.auto_flight.app.wire.FrontendPackets.kt),
// so each CodingKeys case uses its default String raw value.

extension wire.BaseUpdatePacket: @retroactive Decodable {}
extension wire.BaseUpdatePacket: @retroactive Encodable {}
extension wire.BaseUpdatePacket {
    private enum CodingKeys: String, CodingKey {
        case type
    }

    public convenience init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(type: try container.decode(String.self, forKey: .type))
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(type, forKey: .type)
    }
}

extension wire.FlightUpdatePacket: @retroactive Decodable {}
extension wire.FlightUpdatePacket: @retroactive Encodable {}
extension wire.FlightUpdatePacket {
    private enum CodingKeys: String, CodingKey {
        case type
        case basePosition
        case basePositionUpdateTime
        case planePosition
        case planePositionUpdateTime
        case flightRoute
        case flightRouteUpdateTime
        case plannedRoute
        case plannedRouteUpdateTime
    }

    public convenience init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            type: try container.decode(String.self, forKey: .type),
            basePosition: try container.decode(Coordinate.self, forKey: .basePosition),
            basePositionUpdateTime: try container.decode(Int64.self, forKey: .basePositionUpdateTime),
            planePosition: try container.decode(Coordinate.self, forKey: .planePosition),
            planePositionUpdateTime: try container.decode(Int64.self, forKey: .planePositionUpdateTime),
            flightRoute: try container.decode([Coordinate].self, forKey: .flightRoute),
            flightRouteUpdateTime: try container.decode(Int64.self, forKey: .flightRouteUpdateTime),
            plannedRoute: try container.decode([Coordinate].self, forKey: .plannedRoute),
            plannedRouteUpdateTime: try container.decode(Int64.self, forKey: .plannedRouteUpdateTime)
        )
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(type, forKey: .type)
        try container.encode(basePosition, forKey: .basePosition)
        try container.encode(basePositionUpdateTime, forKey: .basePositionUpdateTime)
        try container.encode(planePosition, forKey: .planePosition)
        try container.encode(planePositionUpdateTime, forKey: .planePositionUpdateTime)
        try container.encode(flightRoute, forKey: .flightRoute)
        try container.encode(flightRouteUpdateTime, forKey: .flightRouteUpdateTime)
        try container.encode(plannedRoute, forKey: .plannedRoute)
        try container.encode(plannedRouteUpdateTime, forKey: .plannedRouteUpdateTime)
    }
}

extension wire.ConnectionUpdatePacket: @retroactive Decodable {}
extension wire.ConnectionUpdatePacket: @retroactive Encodable {}
extension wire.ConnectionUpdatePacket {
    private enum CodingKeys: String, CodingKey {
        case type
        case baseConnectionState
        case lastContactBaseStationTimestamp
        case planeConnectionState
        case lastContactPlaneTimestamp
        case gpsConnectionBase
        case gpsConnectionPlane
        case barometerConnectionBase
        case barometerConnectionPlane
        case motorComConnectionPlane
        case magnetometerConnectionPlane
        case accelerometerConnectionPlane
        case manualOverridePlane
        case flightState
    }

    public convenience init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            type: try container.decode(String.self, forKey: .type),
            baseConnectionState: try container.decode(wire.ConnectionState.self, forKey: .baseConnectionState),
            lastContactBaseStationTimestamp: try container.decode(Int64.self, forKey: .lastContactBaseStationTimestamp),
            planeConnectionState: try container.decode(wire.ConnectionState.self, forKey: .planeConnectionState),
            lastContactPlaneTimestamp: try container.decode(Int64.self, forKey: .lastContactPlaneTimestamp),
            gpsConnectionBase: try container.decode(wire.ConnectionState.self, forKey: .gpsConnectionBase),
            gpsConnectionPlane: try container.decode(wire.ConnectionState.self, forKey: .gpsConnectionPlane),
            barometerConnectionBase: try container.decode(wire.ConnectionState.self, forKey: .barometerConnectionBase),
            barometerConnectionPlane: try container.decode(wire.ConnectionState.self, forKey: .barometerConnectionPlane),
            motorComConnectionPlane: try container.decode(wire.ConnectionState.self, forKey: .motorComConnectionPlane),
            magnetometerConnectionPlane: try container.decode(wire.ConnectionState.self, forKey: .magnetometerConnectionPlane),
            accelerometerConnectionPlane: try container.decode(wire.ConnectionState.self, forKey: .accelerometerConnectionPlane),
            manualOverridePlane: try container.decode(Bool.self, forKey: .manualOverridePlane),
            flightState: try container.decode(wire.FlightState.self, forKey: .flightState)
        )
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(type, forKey: .type)
        try container.encode(baseConnectionState, forKey: .baseConnectionState)
        try container.encode(lastContactBaseStationTimestamp, forKey: .lastContactBaseStationTimestamp)
        try container.encode(planeConnectionState, forKey: .planeConnectionState)
        try container.encode(lastContactPlaneTimestamp, forKey: .lastContactPlaneTimestamp)
        try container.encode(gpsConnectionBase, forKey: .gpsConnectionBase)
        try container.encode(gpsConnectionPlane, forKey: .gpsConnectionPlane)
        try container.encode(barometerConnectionBase, forKey: .barometerConnectionBase)
        try container.encode(barometerConnectionPlane, forKey: .barometerConnectionPlane)
        try container.encode(motorComConnectionPlane, forKey: .motorComConnectionPlane)
        try container.encode(magnetometerConnectionPlane, forKey: .magnetometerConnectionPlane)
        try container.encode(accelerometerConnectionPlane, forKey: .accelerometerConnectionPlane)
        try container.encode(manualOverridePlane, forKey: .manualOverridePlane)
        try container.encode(flightState, forKey: .flightState)
    }
}

extension wire.AreaDefinePacket: @retroactive Decodable {}
extension wire.AreaDefinePacket: @retroactive Encodable {}
extension wire.AreaDefinePacket {
    private enum CodingKeys: String, CodingKey {
        case shape
    }

    public convenience init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(shape: try container.decode([Coordinate].self, forKey: .shape))
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(shape, forKey: .shape)
    }
}

extension wire.SensorPacket: @retroactive Decodable {}
extension wire.SensorPacket: @retroactive Encodable {}
extension wire.SensorPacket {
    private enum CodingKeys: String, CodingKey {
        case type
        case barometerPressureBase
        case barometerPressurePlane
        case calculatedAltitude
        case headingPlane
    }

    public convenience init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            type: try container.decode(String.self, forKey: .type),
            barometerPressureBase: try container.decode(Float.self, forKey: .barometerPressureBase),
            barometerPressurePlane: try container.decode(Float.self, forKey: .barometerPressurePlane),
            calculatedAltitude: try container.decode(Float.self, forKey: .calculatedAltitude),
            headingPlane: try container.decode(Int32.self, forKey: .headingPlane)
        )
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(type, forKey: .type)
        try container.encode(barometerPressureBase, forKey: .barometerPressureBase)
        try container.encode(barometerPressurePlane, forKey: .barometerPressurePlane)
        try container.encode(calculatedAltitude, forKey: .calculatedAltitude)
        try container.encode(headingPlane, forKey: .headingPlane)
    }
}

extension wire.BatteryStatusPacket: @retroactive Decodable {}
extension wire.BatteryStatusPacket: @retroactive Encodable {}
extension wire.BatteryStatusPacket {
    private enum CodingKeys: String, CodingKey {
        case type
        case baseBatteryPercentage
        case planeBatteryPercentage
    }

    public convenience init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            type: try container.decode(String.self, forKey: .type),
            baseBatteryPercentage: try container.decode(Int32.self, forKey: .baseBatteryPercentage),
            planeBatteryPercentage: try container.decode(Int32.self, forKey: .planeBatteryPercentage)
        )
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(type, forKey: .type)
        try container.encode(baseBatteryPercentage, forKey: .baseBatteryPercentage)
        try container.encode(planeBatteryPercentage, forKey: .planeBatteryPercentage)
    }
}

extension wire.PlannedRoutePacket: @retroactive Decodable {}
extension wire.PlannedRoutePacket: @retroactive Encodable {}
extension wire.PlannedRoutePacket {
    private enum CodingKeys: String, CodingKey {
        case type
        case route
    }

    public convenience init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            type: try container.decode(String.self, forKey: .type),
            route: try container.decode([Coordinate].self, forKey: .route)
        )
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(type, forKey: .type)
        try container.encode(route, forKey: .route)
    }
}
