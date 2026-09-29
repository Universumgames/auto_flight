//
//  ConfigurationState.swift
//  iosApp
//
//  Created by Tom Arlt on 25.09.26.
//

import SharedLogic

extension ConfigurationState: @retroactive Comparable {
    public static func < (lhs: borrowing ExportedKotlinPackages.de.universegame.auto_flight.app.ConfigurationState, rhs: borrowing ExportedKotlinPackages.de.universegame.auto_flight.app.ConfigurationState) -> Bool {
        return lhs.index < rhs.index
    }
}
