//
//  Data.swift
//  iosApp
//
//  Created by Tom Arlt on 28.08.26.
//

import Foundation

extension Data {
    var asString: String {
        String(decoding: self, as: UTF8.self)
    }

    /// Hex encoding, used to pass raw bytes across the Kotlin/Swift boundary: Swift Export
    /// (Alpha) has no bridge for constructing a Kotlin `ByteArray`, only for reading one, so
    /// `SharedLogic.FrontendPackets`'s codec takes/returns hex `String`s instead (see the
    /// doc comment on `FrontendPackets` in `sharedLogic/.../wire/FrontendPackets.kt`).
    var hexEncoded: String {
        map { String(format: "%02x", $0) }.joined()
    }

    init?(hexEncoded hex: String) {
        guard hex.count.isMultiple(of: 2) else { return nil }
        var bytes = [UInt8]()
        bytes.reserveCapacity(hex.count / 2)
        var index = hex.startIndex
        while index < hex.endIndex {
            let next = hex.index(index, offsetBy: 2)
            guard let byte = UInt8(hex[index..<next], radix: 16) else { return nil }
            bytes.append(byte)
            index = next
        }
        self = Data(bytes)
    }
}
