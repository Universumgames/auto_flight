import Foundation

enum JSONParseError: Error, CustomStringConvertible {
    case unexpectedEnd
    case unexpectedCharacter(Character, offset: Int)
    case invalidNumber(String)
    case invalidEscape(offset: Int)
    case trailingCharacters(offset: Int)

    var description: String {
        switch self {
        case .unexpectedEnd: return "unexpected end of input"
        case let .unexpectedCharacter(c, offset): return "unexpected '\(c)' at offset \(offset)"
        case let .invalidNumber(s): return "invalid number '\(s)'"
        case let .invalidEscape(offset): return "invalid escape sequence at offset \(offset)"
        case let .trailingCharacters(offset): return "trailing characters at offset \(offset)"
        }
    }
}

/// Parses JSON text straight into a `CBORValue`. Used instead of JSONSerialization so integer
/// literals stay CBOR integers (the C++ side reads e.g. `id` as an integer) and object key
/// order is preserved on the wire.
struct JSONParser {
    private let chars: [UInt8]
    private var pos = 0

    static func parse(_ text: String) throws -> CBORValue {
        var parser = JSONParser(chars: Array(text.utf8))
        let value = try parser.parseValue()
        parser.skipWhitespace()
        guard parser.pos == parser.chars.count else { throw JSONParseError.trailingCharacters(offset: parser.pos) }
        return value
    }

    private init(chars: [UInt8]) {
        self.chars = chars
    }

    private mutating func skipWhitespace() {
        while pos < chars.count, [0x20, 0x09, 0x0A, 0x0D].contains(chars[pos]) { pos += 1 }
    }

    private func peek() throws -> UInt8 {
        guard pos < chars.count else { throw JSONParseError.unexpectedEnd }
        return chars[pos]
    }

    private mutating func expect(_ c: UInt8) throws {
        let actual = try peek()
        guard actual == c else { throw JSONParseError.unexpectedCharacter(Character(UnicodeScalar(actual)), offset: pos) }
        pos += 1
    }

    private mutating func expectLiteral(_ literal: String) throws {
        for c in literal.utf8 { try expect(c) }
    }

    private mutating func parseValue() throws -> CBORValue {
        skipWhitespace()
        switch try peek() {
        case UInt8(ascii: "{"): return try parseObject()
        case UInt8(ascii: "["): return try parseArray()
        case UInt8(ascii: "\""): return .text(try parseString())
        case UInt8(ascii: "t"): try expectLiteral("true"); return .bool(true)
        case UInt8(ascii: "f"): try expectLiteral("false"); return .bool(false)
        case UInt8(ascii: "n"): try expectLiteral("null"); return .null
        case UInt8(ascii: "-"), UInt8(ascii: "0") ... UInt8(ascii: "9"): return try parseNumber()
        case let c: throw JSONParseError.unexpectedCharacter(Character(UnicodeScalar(c)), offset: pos)
        }
    }

    private mutating func parseObject() throws -> CBORValue {
        try expect(UInt8(ascii: "{"))
        var pairs: [(CBORValue, CBORValue)] = []
        skipWhitespace()
        if try peek() == UInt8(ascii: "}") { pos += 1; return .map(pairs) }
        while true {
            skipWhitespace()
            let key = try parseString()
            skipWhitespace()
            try expect(UInt8(ascii: ":"))
            pairs.append((.text(key), try parseValue()))
            skipWhitespace()
            if try peek() == UInt8(ascii: ",") { pos += 1; continue }
            try expect(UInt8(ascii: "}"))
            return .map(pairs)
        }
    }

    private mutating func parseArray() throws -> CBORValue {
        try expect(UInt8(ascii: "["))
        var items: [CBORValue] = []
        skipWhitespace()
        if try peek() == UInt8(ascii: "]") { pos += 1; return .array(items) }
        while true {
            items.append(try parseValue())
            skipWhitespace()
            if try peek() == UInt8(ascii: ",") { pos += 1; continue }
            try expect(UInt8(ascii: "]"))
            return .array(items)
        }
    }

    private mutating func parseString() throws -> String {
        try expect(UInt8(ascii: "\""))
        var bytes: [UInt8] = []
        while true {
            let c = try peek()
            pos += 1
            switch c {
            case UInt8(ascii: "\""):
                return String(decoding: bytes, as: UTF8.self)
            case UInt8(ascii: "\\"):
                let escape = try peek()
                pos += 1
                switch escape {
                case UInt8(ascii: "\""): bytes.append(0x22)
                case UInt8(ascii: "\\"): bytes.append(0x5C)
                case UInt8(ascii: "/"): bytes.append(0x2F)
                case UInt8(ascii: "b"): bytes.append(0x08)
                case UInt8(ascii: "f"): bytes.append(0x0C)
                case UInt8(ascii: "n"): bytes.append(0x0A)
                case UInt8(ascii: "r"): bytes.append(0x0D)
                case UInt8(ascii: "t"): bytes.append(0x09)
                case UInt8(ascii: "u"):
                    var scalar = try parseHex4()
                    // Combine a UTF-16 surrogate pair (😀) into one scalar.
                    if (0xD800 ..< 0xDC00).contains(scalar) {
                        try expect(UInt8(ascii: "\\"))
                        try expect(UInt8(ascii: "u"))
                        let low = try parseHex4()
                        guard (0xDC00 ..< 0xE000).contains(low) else { throw JSONParseError.invalidEscape(offset: pos) }
                        scalar = 0x10000 + ((scalar - 0xD800) << 10) + (low - 0xDC00)
                    }
                    guard let u = UnicodeScalar(scalar) else { throw JSONParseError.invalidEscape(offset: pos) }
                    bytes += Array(String(Character(u)).utf8)
                default:
                    throw JSONParseError.invalidEscape(offset: pos - 1)
                }
            default:
                bytes.append(c)
            }
        }
    }

    private mutating func parseHex4() throws -> UInt32 {
        guard pos + 4 <= chars.count,
              let value = UInt32(String(decoding: chars[pos ..< pos + 4], as: UTF8.self), radix: 16) else {
            throw JSONParseError.invalidEscape(offset: pos)
        }
        pos += 4
        return value
    }

    private mutating func parseNumber() throws -> CBORValue {
        let start = pos
        var isInteger = true
        while pos < chars.count {
            let c = chars[pos]
            if c == UInt8(ascii: ".") || c == UInt8(ascii: "e") || c == UInt8(ascii: "E") {
                isInteger = false
            } else if !(c == UInt8(ascii: "-") || c == UInt8(ascii: "+") || (UInt8(ascii: "0") ... UInt8(ascii: "9")).contains(c)) {
                break
            }
            pos += 1
        }
        let text = String(decoding: chars[start ..< pos], as: UTF8.self)
        if isInteger {
            if let n = UInt64(text) { return .unsigned(n) }
            if let n = Int64(text) { return n < 0 ? .negative(n) : .unsigned(UInt64(n)) } // "-0"
        }
        // Also covers integers outside the 64-bit range.
        guard let d = Double(text) else { throw JSONParseError.invalidNumber(text) }
        return .double(d)
    }
}
