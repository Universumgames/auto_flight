//
//  FragmentReassembly.swift
//  iosApp
//

import Foundation

/// Shared framing for the base station's application-level BLE fragmentation
/// (base_station/components/frontend_bluetooth/BluetoothManager.cpp: `sendFragmented` /
/// `handleFragmentedWrite`). Every fragment — for both notify and write — is prefixed
/// with a 4-byte little-endian header `[0:2) totalLength, [2:4) offset]` followed by
/// that offset's chunk of the payload. Reads are the one exception: plain GATT read
/// responses on this project's small, fixed-size characteristics are not framed this way.
enum BLEFragmentFraming {
    static let headerSize = 4

    /// Builds one `[totalLength, offset] + chunk` frame.
    static func makeFrame(totalLength: Int, offset: Int, chunk: Data) -> Data {
        var frame = Data(capacity: headerSize + chunk.count)
        frame.append(UInt8(totalLength & 0xFF))
        frame.append(UInt8((totalLength >> 8) & 0xFF))
        frame.append(UInt8(offset & 0xFF))
        frame.append(UInt8((offset >> 8) & 0xFF))
        frame.append(chunk)
        return frame
    }
}

/// Reassembles BLE notification fragments framed as described by `BLEFragmentFraming`.
struct FragmentReassemblyBuffer {
    private var data = Data()
    private var totalLength = 0
    private var received = 0

    /// Feeds one fragment into the buffer. Returns the fully reassembled payload once
    /// every byte has arrived, or nil while more fragments are still expected.
    mutating func ingest(_ fragment: Data) -> Data? {
        let bytes = [UInt8](fragment)
        guard bytes.count >= BLEFragmentFraming.headerSize else {
            print("BLE fragment too small (\(bytes.count) bytes); dropping")
            return nil
        }

        let fragTotal = Int(bytes[0]) | (Int(bytes[1]) << 8)
        let offset = Int(bytes[2]) | (Int(bytes[3]) << 8)
        let chunk = bytes[BLEFragmentFraming.headerSize...]

        if offset == 0 {
            data = Data(count: fragTotal)
            totalLength = fragTotal
            received = 0
        } else if totalLength != fragTotal || offset > data.count {
            print("BLE fragment doesn't match in-progress message; dropping")
            reset()
            return nil
        }

        if !chunk.isEmpty {
            data.replaceSubrange(offset..<(offset + chunk.count), with: chunk)
        }
        received += chunk.count

        guard received >= totalLength else { return nil }
        defer { reset() }
        return data
    }

    private mutating func reset() {
        data = Data()
        totalLength = 0
        received = 0
    }
}
