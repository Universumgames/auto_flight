//
//  SwiftRouteSettings.swift
//  iosApp
//
//  Created by Tom Arlt on 05.10.26.
//
import SharedLogic

struct SwiftRouteSettings{
    var routeAlgorithm: wire.RouteAlgorithm
    var overlapPercentage: Int
    
    init(algorithm: wire.RouteAlgorithm, overlapPercent: Int) {
        self.routeAlgorithm = algorithm
        self.overlapPercentage = overlapPercent
    }
    
    init(settings: wire.RouteSettings) {
        self.init(algorithm: settings.routeAlgorithm, overlapPercent: Int(settings.overlapPercentage))
    }
    
    func toWire() -> wire.RouteSettings{
        return .init(routeAlgorithm: routeAlgorithm, overlapPercentage: Int32(overlapPercentage))
    }
    
    static var standard: SwiftRouteSettings {
        .init(algorithm: .BOUSTROPHEDON, overlapPercent: 20)
    }
}
