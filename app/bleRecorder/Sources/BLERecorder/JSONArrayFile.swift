import Foundation

/// Appends entries to a JSON array file while keeping it a valid JSON document after every
/// write: each append overwrites the trailing "\n]" and writes it again after the new entry,
/// so a crash or Ctrl-C never leaves a truncated file behind.
final class JSONArrayFile {
    private let handle: FileHandle
    private var count = 0
    private static let closing = Data("\n]\n".utf8)

    init(path: String) throws {
        guard FileManager.default.createFile(atPath: path, contents: Data("[".utf8) + Self.closing) else {
            throw CocoaError(.fileWriteUnknown, userInfo: [NSFilePathErrorKey: path])
        }
        handle = try FileHandle(forWritingTo: URL(fileURLWithPath: path))
    }

    var entryCount: Int { count }

    func append(_ json: String) throws {
        let end = try handle.seekToEnd()
        try handle.seek(toOffset: end - UInt64(Self.closing.count))
        let separator = count == 0 ? "\n" : ",\n"
        // Indent the entry by one level so the file reads like a pretty-printed array.
        let indented = json.split(separator: "\n", omittingEmptySubsequences: false)
            .map { "  " + $0 }
            .joined(separator: "\n")
        try handle.write(contentsOf: Data((separator + indented).utf8) + Self.closing)
        count += 1
    }

    func close() {
        try? handle.synchronize()
        try? handle.close()
    }
}
