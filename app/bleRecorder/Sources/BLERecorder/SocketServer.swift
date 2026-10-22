import Darwin
import Foundation

struct SocketError: Error, CustomStringConvertible {
    let description: String

    /// Builds the message from the current `errno`.
    static func posix(_ what: String) -> SocketError {
        SocketError(description: "\(what): \(String(cString: strerror(errno)))")
    }
}

/// Line-based (newline-delimited JSON) socket server over TCP and/or Unix domain sockets.
/// Everything runs on the main queue, the same queue CoreBluetooth delivers on, so the
/// recorder can use it without locking.
final class SocketServer {
    final class Client {
        let fd: Int32
        let label: String
        fileprivate var readSource: DispatchSourceRead?
        fileprivate var writeSource: DispatchSourceWrite?
        fileprivate var inbox = Data()
        fileprivate var outbox = Data()

        fileprivate init(fd: Int32, label: String) {
            self.fd = fd
            self.label = label
        }
    }

    private struct Listener {
        let fd: Int32
        let source: DispatchSourceRead
        let unixPath: String?
    }

    /// A client that falls this far behind is dropped instead of buffering without bound.
    private static let maxOutboxSize = 4 * 1024 * 1024

    var onLine: ((String, Client) -> Void)?
    private var listeners: [Listener] = []
    private var clients: [Int32: Client] = [:]

    var clientCount: Int { clients.count }

    init() {
        // A client disconnecting mid-write must produce EPIPE, not kill the process.
        signal(SIGPIPE, SIG_IGN)
    }

    // MARK: - Listening

    /// Listens on `host:port`; `host` may be a name or an IPv4/IPv6 address.
    func listenTCP(host: String, port: UInt16) throws {
        var hints = addrinfo()
        hints.ai_family = AF_UNSPEC
        hints.ai_socktype = SOCK_STREAM
        hints.ai_flags = AI_PASSIVE
        var result: UnsafeMutablePointer<addrinfo>?
        let status = getaddrinfo(host, String(port), &hints, &result)
        guard status == 0, let info = result else {
            throw SocketError(description: "cannot resolve \(host): \(String(cString: gai_strerror(status)))")
        }
        defer { freeaddrinfo(result) }

        let fd = socket(info.pointee.ai_family, info.pointee.ai_socktype, info.pointee.ai_protocol)
        guard fd >= 0 else { throw SocketError.posix("socket") }
        var yes: Int32 = 1
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, socklen_t(MemoryLayout<Int32>.size))
        guard bind(fd, info.pointee.ai_addr, info.pointee.ai_addrlen) == 0 else {
            close(fd)
            throw SocketError.posix("bind \(host):\(port)")
        }
        try startListening(fd: fd, unixPath: nil, label: "tcp://\(host):\(port)")
    }

    /// Listens on a Unix domain socket at `path`, replacing a stale socket file left by a previous run.
    func listenUnix(path: String) throws {
        var addr = sockaddr_un()
        addr.sun_family = sa_family_t(AF_UNIX)
        let pathBytes = Array(path.utf8)
        let capacity = MemoryLayout.size(ofValue: addr.sun_path)
        guard pathBytes.count < capacity else {
            throw SocketError(description: "socket path too long (max \(capacity - 1) bytes): \(path)")
        }
        withUnsafeMutableBytes(of: &addr.sun_path) { buffer in
            buffer.copyBytes(from: pathBytes)
            buffer[pathBytes.count] = 0
        }

        var info = stat()
        if lstat(path, &info) == 0 {
            guard (info.st_mode & S_IFMT) == S_IFSOCK else {
                throw SocketError(description: "\(path) exists and is not a socket")
            }
            unlink(path)
        }

        let fd = socket(AF_UNIX, SOCK_STREAM, 0)
        guard fd >= 0 else { throw SocketError.posix("socket") }
        let bound = withUnsafePointer(to: &addr) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                bind(fd, $0, socklen_t(MemoryLayout<sockaddr_un>.size))
            }
        }
        guard bound == 0 else {
            close(fd)
            throw SocketError.posix("bind \(path)")
        }
        try startListening(fd: fd, unixPath: path, label: "unix://\(path)")
    }

    private func startListening(fd: Int32, unixPath: String?, label: String) throws {
        guard listen(fd, 16) == 0 else {
            close(fd)
            throw SocketError.posix("listen \(label)")
        }
        setNonBlocking(fd)
        let source = DispatchSource.makeReadSource(fileDescriptor: fd, queue: .main)
        source.setEventHandler { [weak self] in self?.acceptClients(listenerFD: fd, label: label) }
        source.setCancelHandler { close(fd) }
        source.resume()
        listeners.append(Listener(fd: fd, source: source, unixPath: unixPath))
        log("Listening on \(label)")
    }

    private func acceptClients(listenerFD: Int32, label: String) {
        while true {
            let fd = accept(listenerFD, nil, nil)
            guard fd >= 0 else { return } // EAGAIN: no more pending connections
            setNonBlocking(fd)
            var yes: Int32 = 1
            setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, socklen_t(MemoryLayout<Int32>.size))

            let client = Client(fd: fd, label: "\(label)#\(fd)")
            let source = DispatchSource.makeReadSource(fileDescriptor: fd, queue: .main)
            source.setEventHandler { [weak self, weak client] in
                guard let self, let client else { return }
                self.readAvailable(from: client)
            }
            // Close only once the source is cancelled, so its fd can't be reused while still watched.
            source.setCancelHandler { close(fd) }
            client.readSource = source
            clients[fd] = client
            source.resume()
            log("Client connected (\(client.label), \(clients.count) total)")
        }
    }

    // MARK: - Reading

    private func readAvailable(from client: Client) {
        var buffer = [UInt8](repeating: 0, count: 64 * 1024)
        let n = read(client.fd, &buffer, buffer.count)
        if n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR) {
            disconnect(client)
            return
        }
        guard n > 0 else { return }
        client.inbox.append(contentsOf: buffer[0 ..< n])

        while let newline = client.inbox.firstIndex(of: 0x0A) {
            let lineData = client.inbox[client.inbox.startIndex ..< newline]
            client.inbox.removeSubrange(client.inbox.startIndex ... newline)
            let line = String(decoding: lineData, as: UTF8.self).trimmingCharacters(in: .whitespacesAndNewlines)
            if !line.isEmpty { onLine?(line, client) }
            // The handler may have dropped this client.
            guard clients[client.fd] === client else { return }
        }
    }

    // MARK: - Writing

    /// Sends `line` (newline appended) to every connected client.
    func broadcast(_ line: String) {
        for client in Array(clients.values) { send(line, to: client) }
    }

    /// Sends `line` (newline appended) to a single client.
    func send(_ line: String, to client: Client) {
        guard clients[client.fd] === client else { return }
        client.outbox.append(Data((line + "\n").utf8))
        guard client.outbox.count <= Self.maxOutboxSize else {
            log("Client \(client.label) is not reading fast enough; dropping it")
            disconnect(client)
            return
        }
        flush(client)
    }

    private func flush(_ client: Client) {
        while !client.outbox.isEmpty {
            let written = client.outbox.withUnsafeBytes { write(client.fd, $0.baseAddress, $0.count) }
            if written > 0 {
                client.outbox.removeSubrange(client.outbox.startIndex ..< client.outbox.startIndex + written)
            } else if written < 0 && (errno == EAGAIN || errno == EINTR) {
                // Socket buffer full: retry once the socket is writable again.
                if client.writeSource == nil {
                    let source = DispatchSource.makeWriteSource(fileDescriptor: client.fd, queue: .main)
                    source.setEventHandler { [weak self, weak client] in
                        guard let self, let client else { return }
                        self.flush(client)
                    }
                    client.writeSource = source
                    source.resume()
                }
                return
            } else {
                disconnect(client)
                return
            }
        }
        client.writeSource?.cancel()
        client.writeSource = nil
    }

    // MARK: - Teardown

    private func disconnect(_ client: Client) {
        guard clients.removeValue(forKey: client.fd) != nil else { return }
        client.writeSource?.cancel()
        client.readSource?.cancel() // its cancel handler closes the fd
        log("Client disconnected (\(client.label), \(clients.count) left)")
    }

    func shutdown() {
        for client in Array(clients.values) { disconnect(client) }
        for listener in listeners {
            listener.source.cancel()
            if let path = listener.unixPath { unlink(path) }
        }
        listeners.removeAll()
    }

    private func setNonBlocking(_ fd: Int32) {
        _ = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)
    }
}
