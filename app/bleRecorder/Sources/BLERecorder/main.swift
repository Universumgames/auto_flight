import Foundation

let usage = """
Usage: ble-recorder [-o <file.json>] [-n <name>] [-t <seconds>] [--raw]
                    [--tcp [host:]port] [--socket <path>] [--no-file]

Connects to the first base station advertising the auto_flight BLE service, subscribes to
every characteristic and appends each decoded packet to a JSON array file. Optionally serves
the link over a socket as newline-delimited JSON: received packets are pushed to every client,
and {"characteristic": "AB31", "packet": {...}} lines sent by a client are written to the device
as CBOR (see README.md).

  -o, --output <file>   Output file (default: recording-<timestamp>.json)
  -n, --name <name>     Only connect to a device whose name contains <name>
  -t, --duration <sec>  Stop after <sec> seconds (counted from launch; default: run until Ctrl-C)
      --raw             Also store the raw CBOR payload as hex for each packet
      --tcp [host:]port Serve the JSON passthrough over TCP (default host 127.0.0.1)
      --socket <path>   Serve the JSON passthrough on a Unix domain socket
      --no-file         Don't write a JSON file (requires --tcp or --socket)
  -h, --help            Show this help
"""

var outputPath: String?
var nameFilter: String?
var includeRaw = false
var duration: TimeInterval?
var tcpEndpoint: (host: String, port: UInt16)?
var socketPath: String?
var writeFile = true

func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data("\(message)\n\n\(usage)\n".utf8))
    exit(2)
}

/// Parses "7878", "0.0.0.0:7878" or "[::1]:7878".
func parseEndpoint(_ value: String) -> (host: String, port: UInt16)? {
    guard let colon = value.lastIndex(of: ":") else {
        return UInt16(value).map { ("127.0.0.1", $0) }
    }
    var host = String(value[..<colon])
    if host.hasPrefix("["), host.hasSuffix("]") { host = String(host.dropFirst().dropLast()) }
    guard !host.isEmpty, let port = UInt16(value[value.index(after: colon)...]) else { return nil }
    return (host, port)
}

var args = CommandLine.arguments.dropFirst().makeIterator()
while let arg = args.next() {
    switch arg {
    case "-o", "--output": outputPath = args.next()
    case "-n", "--name": nameFilter = args.next()
    case "-t", "--duration":
        guard let value = args.next().flatMap(TimeInterval.init), value > 0 else {
            fail("\(arg) needs a positive number of seconds")
        }
        duration = value
    case "--tcp":
        guard let endpoint = args.next().flatMap(parseEndpoint) else { fail("--tcp needs [host:]port") }
        tcpEndpoint = endpoint
    case "--socket":
        guard let path = args.next() else { fail("--socket needs a path") }
        socketPath = path
    case "--no-file": writeFile = false
    case "--raw": includeRaw = true
    case "-h", "--help":
        print(usage)
        exit(0)
    default:
        fail("Unknown argument: \(arg)")
    }
}

if !writeFile && tcpEndpoint == nil && socketPath == nil {
    fail("--no-file only makes sense together with --tcp or --socket")
}
if !writeFile && outputPath != nil {
    fail("--no-file and -o can't be combined")
}

let defaultName: String = {
    let f = DateFormatter()
    f.dateFormat = "yyyyMMdd-HHmmss"
    return "recording-\(f.string(from: Date())).json"
}()
let path = outputPath ?? defaultName

var output: JSONArrayFile?
if writeFile {
    do {
        output = try JSONArrayFile(path: path)
    } catch {
        log("Cannot create \(path): \(error.localizedDescription)")
        exit(1)
    }
    log("Writing to \(URL(fileURLWithPath: path).path)")
}

var server: SocketServer?
if tcpEndpoint != nil || socketPath != nil {
    let s = SocketServer()
    do {
        if let tcpEndpoint { try s.listenTCP(host: tcpEndpoint.host, port: tcpEndpoint.port) }
        if let socketPath { try s.listenUnix(path: socketPath) }
    } catch {
        log("Cannot open socket: \(error)")
        s.shutdown()
        exit(1)
    }
    server = s
}

let recorder = Recorder(output: output, server: server, nameFilter: nameFilter, includeRaw: includeRaw)

func stop() -> Never {
    if let output {
        log("Stopping - recorded \(output.entryCount) packets to \(path)")
    } else {
        log("Stopping")
    }
    recorder.shutdown()
    exit(0)
}

if let duration {
    log("Recording for \(duration) seconds")
    DispatchQueue.main.asyncAfter(deadline: .now() + duration) { stop() }
}

// Close the file cleanly on Ctrl-C / kill; it is valid JSON at every point anyway.
var signalSources: [DispatchSourceSignal] = []
for sig in [SIGINT, SIGTERM] {
    signal(sig, SIG_IGN)
    let source = DispatchSource.makeSignalSource(signal: sig, queue: .main)
    source.setEventHandler { stop() }
    source.resume()
    signalSources.append(source)
}

dispatchMain()
