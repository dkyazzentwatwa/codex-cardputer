import Foundation

final class BridgeProcessController: @unchecked Sendable {
    private var process: Process?
    private var outputPipe: Pipe?
    private var errorPipe: Pipe?
    private let outputLines = LineBuffer()
    private let errorLines = LineBuffer()
    private(set) var ownsProcess = false

    var onLifecycle: ((BridgeLifecycle, String?) -> Void)?
    var onManagementReady: ((Int, String) -> Void)?
    var onLogLine: ((String) -> Void)?
    var onUnexpectedExit: ((Int32) -> Void)?

    static let restartDelays: [TimeInterval] = [1, 2, 5, 15]

    func start(runtime: CompanionRuntime, configURL: URL) throws {
        guard process == nil else { return }
        let token = (UUID().uuidString + UUID().uuidString).replacingOccurrences(of: "-", with: "")
        let process = Process()
        let output = Pipe()
        let errors = Pipe()
        process.executableURL = runtime.node
        process.arguments = [runtime.bridgeEntry.path, configURL.path]
        var environment = ProcessInfo.processInfo.environment
        environment["CODEXDECK_CONFIG"] = configURL.path
        environment["CODEXDECK_ADMIN_TOKEN"] = token
        environment["CODEXDECK_ADMIN_PORT"] = "0"
        environment["NODE_ENV"] = "production"
        process.environment = environment
        process.standardOutput = output
        process.standardError = errors
        self.process = process
        outputPipe = output
        errorPipe = errors
        ownsProcess = true
        onLifecycle?(.starting, nil)

        output.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            guard !data.isEmpty else { return }
            for line in self?.outputLines.append(data) ?? [] { self?.consume(line: line, token: token) }
        }
        errors.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            guard !data.isEmpty else { return }
            for line in self?.errorLines.append(data) ?? [] { self?.onLogLine?(line) }
        }
        process.terminationHandler = { [weak self] process in
            DispatchQueue.main.async {
                guard let self else { return }
                self.clearHandles()
                let unexpected = self.ownsProcess
                self.ownsProcess = false
                self.process = nil
                if unexpected { self.onUnexpectedExit?(process.terminationStatus) }
            }
        }
        try process.run()
    }

    func stop(forceAfter delay: TimeInterval = 3, graceful: (() -> Void)? = nil) {
        guard let process else {
            onLifecycle?(.stopped, nil)
            return
        }
        ownsProcess = false
        onLifecycle?(.stopping, nil)
        graceful?()
        DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self, weak process] in
            guard let self, let process, process.isRunning else { return }
            process.terminate()
            DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
                if process.isRunning { kill(process.processIdentifier, SIGKILL) }
            }
            self.clearHandles()
        }
    }

    /// App termination cannot wait for the normal management shutdown round trip.
    /// Stop only the child process we created so it cannot become a stale port owner.
    func terminateForApplicationExit() {
        guard let process, ownsProcess else { return }
        ownsProcess = false
        if process.isRunning {
            kill(process.processIdentifier, SIGTERM)
        }
        clearHandles()
    }

    private func consume(line: String, token: String) {
        onLogLine?(line)
        guard let data = line.data(using: .utf8),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              object["component"] as? String == "management",
              object["event"] as? String == "ready",
              let port = object["port"] as? Int else { return }
        DispatchQueue.main.async { [weak self] in self?.onManagementReady?(port, token) }
    }

    private func clearHandles() {
        outputPipe?.fileHandleForReading.readabilityHandler = nil
        errorPipe?.fileHandleForReading.readabilityHandler = nil
        outputPipe = nil
        errorPipe = nil
    }
}
