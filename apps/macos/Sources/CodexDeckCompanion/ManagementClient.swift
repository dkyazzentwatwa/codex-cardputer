import Foundation

final class ManagementClient {
    private let baseURL: URL
    private let token: String
    private let session: URLSession
    private var socket: URLSessionWebSocketTask?

    var onEvent: (([String: Any]) -> Void)?
    var onDisconnect: ((Error?) -> Void)?

    init(port: Int, token: String, session: URLSession = .shared) {
        baseURL = URL(string: "http://127.0.0.1:\(port)")!
        self.token = token
        self.session = session
    }

    func fetchStatus() async throws -> ManagedBridgeStatus {
        try await get("/v1/status", as: ManagedBridgeStatus.self)
    }

    func fetchDevices() async throws -> [ConnectedDevice] {
        struct Response: Decodable { var devices: [ConnectedDevice] }
        return try await get("/v1/devices", as: Response.self).devices
    }

    func fetchLogs() async throws -> [LogEntry] {
        struct Response: Decodable { var entries: [LogEntry] }
        return try await get("/v1/logs?limit=200", as: Response.self).entries
    }

    func fetchAttention() async throws -> (approvals: [DesktopApproval], inputs: [PendingInput]) {
        struct Response: Decodable {
            var approvals: [DesktopApproval]
            var inputs: [PendingInput]
        }
        let response = try await get("/v1/attention", as: Response.self)
        return (response.approvals, response.inputs)
    }

    func reload() async throws { _ = try await post("/v1/reload", body: [:]) }
    func shutdown() async throws { _ = try await post("/v1/shutdown", body: [:]) }

    func clearFinishedTasks() async throws -> Int {
        struct Response: Decodable { var cleared: Int }
        let data = try await post("/v1/tasks/clear-finished", body: [:])
        return try JSONDecoder().decode(Response.self, from: data).cleared
    }

    func respond(inputID: String, answers: [String: [String]]) async throws {
        let mapped = answers.mapValues { ["answers": $0] }
        _ = try await post("/v1/input/\(inputID)/respond", body: ["answers": mapped])
    }

    func respond(approvalID: String, decision: String) async throws {
        _ = try await post("/v1/approvals/\(approvalID)/respond", body: ["decision": decision])
    }

    func connectEvents() {
        var request = URLRequest(url: URL(string: "ws://127.0.0.1:\(baseURL.port!)/v1/events")!)
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        let socket = session.webSocketTask(with: request)
        self.socket = socket
        socket.resume()
        receive()
    }

    func disconnect() {
        socket?.cancel(with: .goingAway, reason: nil)
        socket = nil
    }

    private func get<T: Decodable>(_ path: String, as type: T.Type) async throws -> T {
        var request = URLRequest(url: URL(string: path, relativeTo: baseURL)!)
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        let (data, response) = try await session.data(for: request)
        try validate(response)
        return try JSONDecoder().decode(type, from: data)
    }

    private func post(_ path: String, body: [String: Any]) async throws -> Data {
        var request = URLRequest(url: URL(string: path, relativeTo: baseURL)!)
        request.httpMethod = "POST"
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONSerialization.data(withJSONObject: body)
        let (data, response) = try await session.data(for: request)
        try validate(response)
        return data
    }

    private func validate(_ response: URLResponse) throws {
        guard let http = response as? HTTPURLResponse, 200..<300 ~= http.statusCode else {
            throw URLError(.badServerResponse)
        }
    }

    private func receive() {
        socket?.receive { [weak self] result in
            guard let self else { return }
            switch result {
            case let .success(message):
                let data: Data?
                switch message {
                case let .data(value): data = value
                case let .string(value): data = value.data(using: .utf8)
                @unknown default: data = nil
                }
                if let data,
                   let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
                    self.onEvent?(object)
                }
                self.receive()
            case let .failure(error):
                self.onDisconnect?(error)
            }
        }
    }
}
