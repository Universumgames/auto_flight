import Foundation
import SwiftUI
@preconcurrency import SharedLogic

/// Native Swift counterpart to `sharedLogic`'s (Android-only) `KtorFlightRepository`.
/// Talks to the base station with `URLSession` directly - see the module-level
/// comment in `sharedLogic/build.gradle.kts` for why the Ktor implementation can't
/// be shared with iOS. Mirrors the same websocket + heartbeat + REST behavior.
@MainActor
@Observable
final class ConnectionManager {
    let state = AppState.shared

    var host: String {
        get {
            access(keyPath: \.host)
            return UserDefaults.standard.string(forKey: "host") ?? ""
        }
        set {
            withMutation(keyPath: \.host) {
                UserDefaults.standard.setValue(newValue, forKey: "host")
            }
        }
    }
    
    private var shouldReconnect = false
    private var webSocketTask: URLSessionWebSocketTask?
    private var pingTimer: Timer?
    private var lastPongAt = Date.distantPast
    private var reconnectTask: Task<Void, Never>?
    
    init(){
        
    }

    func connect(host: String) {
        self.host = host
        shouldReconnect = true
        reconnectTask?.cancel()
        openSocket()
    }

    func disconnect() {
        shouldReconnect = false
        reconnectTask?.cancel()
        pingTimer?.invalidate()
        pingTimer = nil
        webSocketTask?.cancel(with: .goingAway, reason: nil)
        webSocketTask = nil
        state.connectionStateBaseStation = .CONNECTING
    }

    private func openSocket() {
        guard shouldReconnect, !host.isEmpty, let url = URL(string: "ws://\(host)/api/ws") else { return }

        let task = URLSession.shared.webSocketTask(with: url)
        webSocketTask = task
        task.resume()

        lastPongAt = Date()
        state.connectionStateBaseStation = .CONNECTING

        pingTimer?.invalidate()
        pingTimer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.sendPingAndCheckHeartbeat() }
        }

        listen(on: task)
    }

    private func listen(on task: URLSessionWebSocketTask) {
        task.receive { [weak self] result in
            Task { @MainActor in
                guard let self, self.webSocketTask === task else { return }
                switch result {
                case .failure:
                    self.handleDisconnect()
                case .success(let message):
                    self.state.connectionStateBaseStation = .CONNECTED
                    if case .string(let text) = message {
                        self.handleFrame(text)
                    }
                    self.listen(on: task)
                }
            }
        }
    }

    private func sendPingAndCheckHeartbeat() {
        guard let task = webSocketTask else { return }
        task.send(.string("ping")) { _ in }
        if Date().timeIntervalSince(lastPongAt) > 5.0 {
            handleDisconnect()
        }
    }

    private func handleFrame(_ raw: String) {
        let normalized = PacketParsing.normalize(raw)
        guard !normalized.isEmpty else { return }

        if normalized == "pong" {
            lastPongAt = Date()
            state.connectionStateBaseStation = .CONNECTED
            return
        }

        guard let data = normalized.data(using: .utf8),
              let json = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any],
              let type = json["type"] as? String else { return }

        switch type {
        case "flight": PacketParsing.applyFlightPacket(state, json)
        case "connection": PacketParsing.applyConnectionPacket(state, json)
        case "sensor": PacketParsing.applySensorPacket(state, json)
        default: break
        }
    }

    private func handleDisconnect() {
        pingTimer?.invalidate()
        pingTimer = nil
        webSocketTask?.cancel(with: .abnormalClosure, reason: nil)
        webSocketTask = nil
        state.connectionStateBaseStation = .CONNECTING
        state.planes.values.forEach { $0.connectionState = .CONNECTING }

        guard shouldReconnect else { return }
        reconnectTask?.cancel()
        reconnectTask = Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: 2_000_000_000)
            guard !Task.isCancelled else { return }
            self?.openSocket()
        }
    }

    // MARK: - REST

    func fetchArea(onResult: @escaping ([Coordinate]?) -> Void) {
        guard let url = URL(string: "http://\(host)/api/area") else { onResult(nil); return }
        URLSession.shared.dataTask(with: url) { data, response, _ in
            var result: [Coordinate]?
            if let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode), let data,
               let json = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] {
                result = PacketParsing.toRoute(json["shape"])
            }
            Task { @MainActor in onResult(result) }
        }.resume()
    }

    func submitArea(polygon: [Coordinate], onResult: @escaping (AreaSubmitResult) -> Void) {
        guard let url = URL(string: "http://\(host)/api/area") else { onResult(.FAILED); return }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let shape = polygon.map { ["latitude": $0.latitude, "longitude": $0.longitude] }
        request.httpBody = try? JSONSerialization.data(withJSONObject: ["shape": shape])

        URLSession.shared.dataTask(with: request) { data, response, _ in
            var result = AreaSubmitResult.FAILED
            if let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode),
               let data, let text = String(data: data, encoding: .utf8),
               let count = Int(text.trimmingCharacters(in: .whitespacesAndNewlines)) {
                result = count == polygon.count ? .ACCEPTED : .MISMATCH
            }
            Task { @MainActor in onResult(result) }
        }.resume()
    }

    func fetchRoute(onResult: @escaping ([Coordinate]) -> Void) {
        guard let url = URL(string: "http://\(host)/api/route") else { onResult([]); return }
        URLSession.shared.dataTask(with: url) { data, response, _ in
            var result: [Coordinate] = []
            if let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode), let data,
               let json = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] {
                result = PacketParsing.toRoute(json["route"]) ?? []
            }
            Task { @MainActor in onResult(result) }
        }.resume()
    }
}
