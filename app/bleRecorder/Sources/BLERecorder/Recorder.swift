import CoreBluetooth
import Foundation

// Mirrors iosApp/iosApp/Models/BluetoothUUIDs.generated.swift (generated from
// shared_components/flight_com/ble_protocol.json). Kept as a plain copy so this tool has no
// dependency on the SharedLogic framework; unknown characteristics are still recorded by UUID.
let serviceUUID = CBUUID(string: "ABF0")
let fragmentHeaderSize = 4

let topicNames: [CBUUID: String] = [
    CBUUID(string: "AB10"): "sensorUpdate",
    CBUUID(string: "AB11"): "position",
    CBUUID(string: "AB12"): "componentStatus",
    CBUUID(string: "AB30"): "plannedRoute",
    CBUUID(string: "AB31"): "plannedArea",
    CBUUID(string: "AB32"): "routeHistory",
    CBUUID(string: "AB33"): "routeHistoryRequest",
    CBUUID(string: "AB34"): "plannedRouteConfirmation",
    CBUUID(string: "AB40"): "flightData",
    CBUUID(string: "ABFF"): "dataRequest",
]

/// Same framing as iosApp's FragmentReassemblyBuffer: `[0:2) totalLength, [2:4) offset`
/// little-endian header followed by the chunk at that offset.
struct FragmentReassemblyBuffer {
    private var data = Data()
    private var totalLength = 0
    private var received = 0

    mutating func ingest(_ fragment: Data) -> Data? {
        let bytes = [UInt8](fragment)
        guard bytes.count >= fragmentHeaderSize else { return nil }

        let fragTotal = Int(bytes[0]) | (Int(bytes[1]) << 8)
        let offset = Int(bytes[2]) | (Int(bytes[3]) << 8)
        let chunk = bytes[fragmentHeaderSize...]

        if offset == 0 {
            data = Data(count: fragTotal)
            totalLength = fragTotal
            received = 0
        } else if totalLength != fragTotal || offset > data.count {
            log("BLE fragment doesn't match in-progress message; dropping")
            reset()
            return nil
        }
        guard offset + chunk.count <= data.count else {
            log("BLE fragment overruns declared length; dropping")
            reset()
            return nil
        }

        if !chunk.isEmpty {
            data.replaceSubrange(offset ..< (offset + chunk.count), with: chunk)
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

func log(_ message: String) {
    FileHandle.standardError.write(Data("[\(ISO8601DateFormatter().string(from: Date()))] \(message)\n".utf8))
}

/// Builds one `[totalLength, offset] + chunk` frame, like iosApp's `BLEFragmentFraming.makeFrame`.
func makeFragmentFrame(totalLength: Int, offset: Int, chunk: Data) -> Data {
    var frame = Data(capacity: fragmentHeaderSize + chunk.count)
    frame.append(UInt8(totalLength & 0xFF))
    frame.append(UInt8((totalLength >> 8) & 0xFF))
    frame.append(UInt8(offset & 0xFF))
    frame.append(UInt8((offset >> 8) & 0xFF))
    frame.append(chunk)
    return frame
}

final class Recorder: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private let output: JSONArrayFile?
    private let server: SocketServer?
    private let nameFilter: String?
    private let includeRaw: Bool
    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var isReady = false
    private var characteristics: [CBUUID: CBCharacteristic] = [:]
    private var reassemblyBuffers: [CBUUID: FragmentReassemblyBuffer] = [:]
    private let timestampFormatter: ISO8601DateFormatter = {
        let f = ISO8601DateFormatter()
        f.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return f
    }()

    /// One fragment waiting to be written, plus the completion of the message it ends (if it's the last one).
    private struct PendingFrame {
        let frame: Data
        let characteristic: CBCharacteristic
        let completion: ((Error?) -> Void)?
    }

    /// Write-with-response allows one outstanding write at a time; queueing whole messages here
    /// also keeps fragments of messages from different clients from interleaving.
    private var writeQueue: [PendingFrame] = []
    private var writeInFlight = false

    init(output: JSONArrayFile?, server: SocketServer?, nameFilter: String?, includeRaw: Bool) {
        self.output = output
        self.server = server
        self.nameFilter = nameFilter
        self.includeRaw = includeRaw
        super.init()
        server?.onLine = { [weak self] line, client in self?.handleClientLine(line, from: client) }
        central = CBCentralManager(delegate: self, queue: .main)
    }

    func shutdown() {
        if let peripheral { central.cancelPeripheralConnection(peripheral) }
        output?.close()
        server?.shutdown()
    }

    private func startScan() {
        log("Scanning for devices advertising service \(serviceUUID)\(nameFilter.map { " with name containing \"\($0)\"" } ?? "")…")
        central.scanForPeripherals(withServices: [serviceUUID])
    }

    // MARK: - CBCentralManagerDelegate

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            startScan()
        case .unauthorized:
            log("Bluetooth permission denied - allow your terminal under System Settings > Privacy & Security > Bluetooth.")
            exit(1)
        case .unsupported:
            log("Bluetooth LE is not supported on this machine.")
            exit(1)
        default:
            log("Bluetooth state: \(central.state.rawValue) - waiting for it to power on")
        }
    }

    func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        guard self.peripheral == nil else { return }
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? "Unknown Device"
        if let nameFilter, !name.localizedCaseInsensitiveContains(nameFilter) { return }

        log("Found \"\(name)\" (\(peripheral.identifier), RSSI \(RSSI)) - connecting")
        central.stopScan()
        self.peripheral = peripheral
        peripheral.delegate = self
        central.connect(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        log("Connected, discovering services")
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        log("Failed to connect: \(error?.localizedDescription ?? "unknown error") - rescanning")
        self.peripheral = nil
        startScan()
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        log("Disconnected\(error.map { ": \($0.localizedDescription)" } ?? "") - reconnecting")
        isReady = false
        characteristics.removeAll()
        reassemblyBuffers.removeAll()
        failPendingWrites(SocketError(description: "device disconnected"))
        // A pending connect retries until the device shows up again, like the iOS app does.
        central.connect(peripheral)
    }

    // MARK: - CBPeripheralDelegate

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard error == nil, let service = peripheral.services?.first(where: { $0.uuid == serviceUUID }) else {
            log("Service \(serviceUUID) not found: \(error?.localizedDescription ?? "missing")")
            return
        }
        // nil: discover every characteristic, so topics added after this tool was written get recorded too.
        peripheral.discoverCharacteristics(nil, for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard error == nil else {
            log("Characteristic discovery failed: \(error!.localizedDescription)")
            return
        }
        for characteristic in service.characteristics ?? [] {
            characteristics[characteristic.uuid] = characteristic
            let props = characteristic.properties
            if props.contains(.notify) || props.contains(.indicate) {
                peripheral.setNotifyValue(true, for: characteristic)
            }
            // A read is the protocol's "send me the current value now" signal; the
            // reply arrives as a regular fragmented notification.
            if props.contains(.read) {
                peripheral.readValue(for: characteristic)
            }
        }
        isReady = true
        log("Subscribed to \(service.characteristics?.count ?? 0) characteristics, recording (Ctrl-C to stop)")
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil, let raw = characteristic.value else { return }
        let uuid = characteristic.uuid
        var buffer = reassemblyBuffers[uuid] ?? FragmentReassemblyBuffer()
        let complete = buffer.ingest(raw)
        reassemblyBuffers[uuid] = buffer
        if let complete { record(complete, from: uuid) }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        writeInFlight = false
        guard !writeQueue.isEmpty else { return }
        let finished = writeQueue.removeFirst()
        if let error, finished.completion == nil {
            // A middle fragment failed: drop the rest of this message (the base station
            // discards a partial message anyway) and report the error for it once.
            while !writeQueue.isEmpty {
                let next = writeQueue.removeFirst()
                if let completion = next.completion {
                    completion(error)
                    break
                }
            }
        } else {
            finished.completion?(error)
        }
        sendNextFrame()
    }

    // MARK: - Writing

    /// Writes `data` to `uuid`, split into fragments that fit the negotiated ATT MTU and
    /// reassembled by the base station (BluetoothManager::handleFragmentedWrite), mirroring the
    /// iOS app's writeCharacteristic. `completion` runs once the last fragment is acknowledged.
    func write(_ data: Data, to uuid: CBUUID, completion: @escaping (Error?) -> Void) {
        guard isReady, let peripheral, let characteristic = characteristics[uuid] else {
            completion(SocketError(description: isReady ? "characteristic \(uuid) not found on device" : "not connected"))
            return
        }
        guard data.count <= Int(UInt16.max) else {
            completion(SocketError(description: "payload too large (\(data.count) bytes)"))
            return
        }
        let chunkSize = peripheral.maximumWriteValueLength(for: .withResponse) - fragmentHeaderSize
        guard chunkSize > 0 else {
            completion(SocketError(description: "MTU too small"))
            return
        }

        var offset = 0
        repeat {
            let length = min(chunkSize, data.count - offset)
            let chunk = data.subdata(in: offset ..< offset + length)
            offset += length
            writeQueue.append(PendingFrame(
                frame: makeFragmentFrame(totalLength: data.count, offset: offset - length, chunk: chunk),
                characteristic: characteristic,
                completion: offset >= data.count ? completion : nil
            ))
        } while offset < data.count
        sendNextFrame()
    }

    private func sendNextFrame() {
        guard !writeInFlight, let next = writeQueue.first, let peripheral else { return }
        writeInFlight = true
        peripheral.writeValue(next.frame, for: next.characteristic, type: .withResponse)
    }

    private func failPendingWrites(_ error: Error) {
        let pending = writeQueue
        writeQueue.removeAll()
        writeInFlight = false
        pending.forEach { $0.completion?(error) }
    }

    // MARK: - Socket clients

    /// Handles one JSON line from a socket client: `{"characteristic": "AB31", "packet": {...}}`
    /// writes the packet as CBOR; without `packet` it requests a fresh value via a GATT read.
    private func handleClientLine(_ line: String, from client: SocketServer.Client) {
        let message: CBORValue
        do {
            message = try JSONParser.parse(line)
        } catch {
            reply(to: client, [("error", .text("invalid JSON: \(error)"))])
            return
        }
        let seq = message["seq"]
        func respond(_ fields: [(String, CBORValue)]) {
            reply(to: client, fields + (seq.map { [("seq", $0)] } ?? []))
        }

        guard case .map = message else {
            respond([("error", .text("expected a JSON object"))])
            return
        }
        let uuid: CBUUID
        if case let .text(id)? = message["characteristic"] {
            guard id.count == 4 || id.count == 36, id.allSatisfy({ $0.isHexDigit || $0 == "-" }) else {
                respond([("error", .text("invalid characteristic UUID \(id)"))])
                return
            }
            uuid = CBUUID(string: id)
        } else if case let .text(topic)? = message["topic"], let match = topicNames.first(where: { $0.value == topic }) {
            uuid = match.key
        } else {
            respond([("error", .text("missing \"characteristic\" (or a known \"topic\")"))])
            return
        }

        guard let packet = message["packet"] else {
            guard isReady, let peripheral, let characteristic = characteristics[uuid] else {
                respond([("error", .text(isReady ? "characteristic \(uuid.uuidString) not found on device" : "not connected"))])
                return
            }
            peripheral.readValue(for: characteristic)
            respond([("ack", .text(uuid.uuidString))])
            return
        }

        let payload = packet.encoded()
        log("Writing \(topicNames[uuid] ?? "unknown") (\(payload.count) bytes) from \(client.label)")
        write(payload, to: uuid) { error in
            if let error {
                respond([("error", .text(String(describing: error)))])
            } else {
                respond([("ack", .text(uuid.uuidString))])
            }
        }
    }

    private func reply(to client: SocketServer.Client, _ fields: [(String, CBORValue)]) {
        server?.send(CBORValue.map(fields.map { (.text($0.0), $0.1) }).jsonString(pretty: false), to: client)
    }

    // MARK: - Recording

    private func record(_ payload: Data, from uuid: CBUUID) {
        let topic = topicNames[uuid] ?? "unknown"
        var fields: [(String, CBORValue)] = [
            ("receivedAt", .text(timestampFormatter.string(from: Date()))),
            ("characteristic", .text(uuid.uuidString)),
            ("topic", .text(topic)),
        ]
        do {
            let packet = try CBORDecoder.decode(payload)
            fields.append(("packet", packet))
            if includeRaw { fields.append(("raw", .text(payload.hex))) }
            let id = packet["id"]?.intValue.map(String.init) ?? "?"
            log("\(topic) from \(id) (\(payload.count) bytes)")
        } catch {
            fields.append(("error", .text(String(describing: error))))
            fields.append(("raw", .text(payload.hex)))
            log("\(topic): failed to decode \(payload.count) bytes: \(error)")
        }

        let entry = CBORValue.map(fields.map { (.text($0.0), $0.1) })
        server?.broadcast(entry.jsonString(pretty: false))
        guard let output else { return }
        do {
            try output.append(entry.jsonString())
        } catch {
            log("Failed to write to output file: \(error)")
        }
    }
}

extension Data {
    var hex: String { map { String(format: "%02x", $0) }.joined() }
}
