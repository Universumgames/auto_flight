import Foundation

/// Generic CBOR value. The base station encodes every packet as a CBOR map keyed by field
/// name (nlohmann::json::to_cbor, see shared_components/flight_com/PacketCodec.cpp), so a
/// schema-less decode already yields the full, named packet contents.
indirect enum CBORValue {
    case unsigned(UInt64)
    case negative(Int64)
    /// Negative value below Int64.min, stored as the raw CBOR argument (value = -1 - n).
    case bigNegative(UInt64)
    case bytes(Data)
    case text(String)
    case array([CBORValue])
    /// Kept as an ordered list so JSON output preserves the wire field order.
    case map([(CBORValue, CBORValue)])
    case tagged(UInt64, CBORValue)
    case bool(Bool)
    case null
    case undefined
    case half(Float)
    case float(Float)
    case double(Double)
    case simple(UInt8)
}

enum CBORError: Error, CustomStringConvertible {
    case unexpectedEnd
    case invalidAdditionalInfo(UInt8)
    case unexpectedBreak
    case invalidUTF8
    case trailingBytes(Int)

    var description: String {
        switch self {
        case .unexpectedEnd: return "unexpected end of CBOR data"
        case let .invalidAdditionalInfo(info): return "invalid CBOR additional info \(info)"
        case .unexpectedBreak: return "unexpected CBOR break marker"
        case .invalidUTF8: return "CBOR text string is not valid UTF-8"
        case let .trailingBytes(n): return "\(n) trailing bytes after CBOR item"
        }
    }
}

struct CBORDecoder {
    private let bytes: [UInt8]
    private var pos = 0

    static func decode(_ data: Data) throws -> CBORValue {
        var decoder = CBORDecoder(bytes: [UInt8](data))
        let value = try decoder.readItem()
        guard decoder.pos == decoder.bytes.count else {
            throw CBORError.trailingBytes(decoder.bytes.count - decoder.pos)
        }
        return value
    }

    private init(bytes: [UInt8]) {
        self.bytes = bytes
    }

    private mutating func readByte() throws -> UInt8 {
        guard pos < bytes.count else { throw CBORError.unexpectedEnd }
        defer { pos += 1 }
        return bytes[pos]
    }

    private mutating func readUInt(byteCount: Int) throws -> UInt64 {
        guard pos + byteCount <= bytes.count else { throw CBORError.unexpectedEnd }
        var value: UInt64 = 0
        for _ in 0 ..< byteCount {
            value = (value << 8) | UInt64(bytes[pos])
            pos += 1
        }
        return value
    }

    private mutating func readBytes(_ count: Int) throws -> [UInt8] {
        guard count >= 0, pos + count <= bytes.count else { throw CBORError.unexpectedEnd }
        defer { pos += count }
        return Array(bytes[pos ..< pos + count])
    }

    /// Reads the argument for additional info 0...27; nil means indefinite length (31).
    private mutating func readArgument(_ info: UInt8) throws -> UInt64? {
        switch info {
        case 0 ... 23: return UInt64(info)
        case 24: return try readUInt(byteCount: 1)
        case 25: return try readUInt(byteCount: 2)
        case 26: return try readUInt(byteCount: 4)
        case 27: return try readUInt(byteCount: 8)
        case 31: return nil
        default: throw CBORError.invalidAdditionalInfo(info)
        }
    }

    private func isBreak() -> Bool {
        pos < bytes.count && bytes[pos] == 0xFF
    }

    private mutating func readItem() throws -> CBORValue {
        let initial = try readByte()
        let major = initial >> 5
        let info = initial & 0x1F

        switch major {
        case 0:
            guard let n = try readArgument(info) else { throw CBORError.invalidAdditionalInfo(info) }
            return .unsigned(n)
        case 1:
            guard let n = try readArgument(info) else { throw CBORError.invalidAdditionalInfo(info) }
            return n <= UInt64(Int64.max) ? .negative(-1 - Int64(n)) : .bigNegative(n)
        case 2, 3:
            var chunk: [UInt8] = []
            if let length = try readArgument(info) {
                chunk = try readBytes(Int(length))
            } else {
                while !isBreak() {
                    // Indefinite strings are a sequence of definite chunks of the same major type.
                    let header = try readByte()
                    guard let length = try readArgument(header & 0x1F) else {
                        throw CBORError.invalidAdditionalInfo(header & 0x1F)
                    }
                    chunk += try readBytes(Int(length))
                }
                pos += 1
            }
            if major == 2 { return .bytes(Data(chunk)) }
            guard let string = String(bytes: chunk, encoding: .utf8) else { throw CBORError.invalidUTF8 }
            return .text(string)
        case 4:
            var items: [CBORValue] = []
            if let count = try readArgument(info) {
                for _ in 0 ..< count { items.append(try readItem()) }
            } else {
                while !isBreak() { items.append(try readItem()) }
                pos += 1
            }
            return .array(items)
        case 5:
            var pairs: [(CBORValue, CBORValue)] = []
            if let count = try readArgument(info) {
                for _ in 0 ..< count { pairs.append((try readItem(), try readItem())) }
            } else {
                while !isBreak() { pairs.append((try readItem(), try readItem())) }
                pos += 1
            }
            return .map(pairs)
        case 6:
            guard let tag = try readArgument(info) else { throw CBORError.invalidAdditionalInfo(info) }
            return .tagged(tag, try readItem())
        default:
            switch info {
            case 20: return .bool(false)
            case 21: return .bool(true)
            case 22: return .null
            case 23: return .undefined
            case 24: return .simple(try readByte())
            case 25: return .half(Self.halfToFloat(UInt16(try readUInt(byteCount: 2))))
            case 26: return .float(Float(bitPattern: UInt32(try readUInt(byteCount: 4))))
            case 27: return .double(Double(bitPattern: try readUInt(byteCount: 8)))
            case 31: throw CBORError.unexpectedBreak
            case 0 ... 19: return .simple(info)
            default: throw CBORError.invalidAdditionalInfo(info)
            }
        }
    }

    private static func halfToFloat(_ bits: UInt16) -> Float {
        let sign: Float = (bits & 0x8000) != 0 ? -1 : 1
        let exponent = Int((bits >> 10) & 0x1F)
        let mantissa = Float(bits & 0x3FF)
        switch exponent {
        case 0: return sign * mantissa * powf(2, -24)
        case 31: return mantissa == 0 ? sign * .infinity : .nan
        default: return sign * (1 + mantissa / 1024) * powf(2, Float(exponent - 15))
        }
    }
}

extension CBORValue {
    /// Serializes to JSON text, preserving map key order. Byte strings become hex strings,
    /// non-string map keys are stringified, and non-finite floats become null (JSON has none).
    func jsonString(indent: Int = 0, pretty: Bool = true) -> String {
        let pad = pretty ? String(repeating: "  ", count: indent + 1) : ""
        let closePad = pretty ? String(repeating: "  ", count: indent) : ""
        let newline = pretty ? "\n" : ""
        let colon = pretty ? ": " : ":"

        switch self {
        case let .unsigned(n): return String(n)
        case let .negative(n): return String(n)
        case let .bigNegative(n): return n == .max ? "-18446744073709551616" : "-\(n + 1)"
        case let .bytes(data): return Self.quote(data.map { String(format: "%02x", $0) }.joined())
        case let .text(s): return Self.quote(s)
        case let .array(items):
            if items.isEmpty { return "[]" }
            let body = items.map { pad + $0.jsonString(indent: indent + 1, pretty: pretty) }
            return "[" + newline + body.joined(separator: "," + newline) + newline + closePad + "]"
        case let .map(pairs):
            if pairs.isEmpty { return "{}" }
            let body = pairs.map { key, value in
                pad + Self.quote(key.keyString) + colon + value.jsonString(indent: indent + 1, pretty: pretty)
            }
            return "{" + newline + body.joined(separator: "," + newline) + newline + closePad + "}"
        case let .tagged(_, value): return value.jsonString(indent: indent, pretty: pretty)
        case let .bool(b): return b ? "true" : "false"
        case .null, .undefined: return "null"
        // Float description gives the shortest round-tripping form ("0.1", not "0.100000001").
        case let .half(f), let .float(f): return f.isFinite ? f.description : "null"
        case let .double(d): return d.isFinite ? d.description : "null"
        case let .simple(v): return String(v)
        }
    }

    var keyString: String {
        if case let .text(s) = self { return s }
        return jsonString(pretty: false)
    }

    /// Looks up a field of a map value by its string key.
    subscript(key: String) -> CBORValue? {
        guard case let .map(pairs) = self else { return nil }
        return pairs.first { $0.0.keyString == key }?.1
    }

    var intValue: Int64? {
        switch self {
        case let .unsigned(n): return n <= UInt64(Int64.max) ? Int64(n) : nil
        case let .negative(n): return n
        default: return nil
        }
    }

    static func quote(_ s: String) -> String {
        var out = "\""
        for scalar in s.unicodeScalars {
            switch scalar {
            case "\"": out += "\\\""
            case "\\": out += "\\\\"
            case "\n": out += "\\n"
            case "\r": out += "\\r"
            case "\t": out += "\\t"
            case _ where scalar.value < 0x20: out += String(format: "\\u%04x", scalar.value)
            default: out.unicodeScalars.append(scalar)
            }
        }
        return out + "\""
    }
}

extension CBORValue {
    /// Encodes to CBOR the way nlohmann::json::to_cbor does: definite lengths, shortest
    /// integer form. Doubles stay float64; nlohmann's get<float>/get<int> converts on the C++ side.
    func encoded() -> Data {
        var out = Data()
        encode(into: &out)
        return out
    }

    private static func appendHead(_ major: UInt8, _ argument: UInt64, to out: inout Data) {
        let m = major << 5
        switch argument {
        case 0 ..< 24:
            out.append(m | UInt8(argument))
        case 24 ... 0xFF:
            out.append(m | 24)
            out.append(UInt8(argument))
        case 0x100 ... 0xFFFF:
            out.append(m | 25)
            appendBigEndian(argument, byteCount: 2, to: &out)
        case 0x1_0000 ... 0xFFFF_FFFF:
            out.append(m | 26)
            appendBigEndian(argument, byteCount: 4, to: &out)
        default:
            out.append(m | 27)
            appendBigEndian(argument, byteCount: 8, to: &out)
        }
    }

    private static func appendBigEndian(_ value: UInt64, byteCount: Int, to out: inout Data) {
        for i in stride(from: byteCount - 1, through: 0, by: -1) {
            out.append(UInt8((value >> (UInt64(i) * 8)) & 0xFF))
        }
    }

    private func encode(into out: inout Data) {
        switch self {
        case let .unsigned(n): Self.appendHead(0, n, to: &out)
        case let .negative(n): Self.appendHead(1, UInt64(-1 - n), to: &out)
        case let .bigNegative(n): Self.appendHead(1, n, to: &out)
        case let .bytes(data):
            Self.appendHead(2, UInt64(data.count), to: &out)
            out.append(data)
        case let .text(s):
            let utf8 = Data(s.utf8)
            Self.appendHead(3, UInt64(utf8.count), to: &out)
            out.append(utf8)
        case let .array(items):
            Self.appendHead(4, UInt64(items.count), to: &out)
            for item in items { item.encode(into: &out) }
        case let .map(pairs):
            Self.appendHead(5, UInt64(pairs.count), to: &out)
            for (key, value) in pairs {
                key.encode(into: &out)
                value.encode(into: &out)
            }
        case let .tagged(tag, value):
            Self.appendHead(6, tag, to: &out)
            value.encode(into: &out)
        case let .bool(b): out.append(b ? 0xF5 : 0xF4)
        case .null: out.append(0xF6)
        case .undefined: out.append(0xF7)
        case let .half(f), let .float(f):
            out.append(0xFA)
            Self.appendBigEndian(UInt64(f.bitPattern), byteCount: 4, to: &out)
        case let .double(d):
            out.append(0xFB)
            Self.appendBigEndian(d.bitPattern, byteCount: 8, to: &out)
        case let .simple(v):
            if v < 24 { out.append(0xE0 | v) } else { out.append(0xF8); out.append(v) }
        }
    }
}
