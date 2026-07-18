import Foundation

struct ConfigStore {
    let directory: URL

    init(directory: URL? = nil) {
        if let directory {
            self.directory = directory
        } else {
            self.directory = FileManager.default.homeDirectoryForCurrentUser
                .appendingPathComponent("Library/Application Support/CardPuter Codex Control Deck", isDirectory: true)
        }
    }

    var settingsURL: URL { directory.appendingPathComponent("settings.json") }
    var workflowsURL: URL { directory.appendingPathComponent("workflows.json") }
    var logsURL: URL { directory.appendingPathComponent("logs", isDirectory: true) }

    func loadOrCreate(codexPath: String) throws -> (ManagedSettings, ManagedWorkflowRegistry) {
        try ensureDirectory()
        if !FileManager.default.fileExists(atPath: settingsURL.path) {
            var settings = ManagedSettings()
            settings.codexPath = codexPath
            try save(settings: settings, workflows: ManagedWorkflowRegistry())
        }
        return (try decode(ManagedSettings.self, from: settingsURL), try decode(ManagedWorkflowRegistry.self, from: workflowsURL))
    }

    func save(settings: ManagedSettings, workflows: ManagedWorkflowRegistry) throws {
        try ensureDirectory()
        try encode(settings, to: settingsURL)
        try encode(workflows, to: workflowsURL)
    }

    func legacyConfiguration() -> (bridge: URL, workflows: URL)? {
        let environmentRoot = ProcessInfo.processInfo.environment["CODEXDECK_REPO_ROOT"].map(URL.init(fileURLWithPath:))
        let candidates = [
            environmentRoot,
            URL(fileURLWithPath: FileManager.default.currentDirectoryPath),
            FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Documents/GitHub/codex-cardputer"),
        ].compactMap { $0 }
        for root in candidates {
            let bridge = root.appendingPathComponent("apps/bridge/config/bridge.local.yaml")
            let workflows = root.appendingPathComponent("apps/bridge/config/workflows.local.yaml")
            if FileManager.default.fileExists(atPath: bridge.path),
               FileManager.default.fileExists(atPath: workflows.path) {
                return (bridge, workflows)
            }
        }
        return nil
    }

    func migrate(node: URL, helper: URL, legacy: (bridge: URL, workflows: URL)) throws {
        try ensureDirectory()
        let process = Process()
        process.executableURL = node
        process.arguments = [helper.path, legacy.bridge.path, legacy.workflows.path, directory.path]
        let errors = Pipe()
        process.standardError = errors
        try process.run()
        process.waitUntilExit()
        guard process.terminationStatus == 0 else {
            let data = errors.fileHandleForReading.readDataToEndOfFile()
            throw NSError(domain: "CodexDeck.Migration", code: Int(process.terminationStatus), userInfo: [
                NSLocalizedDescriptionKey: String(data: data, encoding: .utf8) ?? "Configuration migration failed",
            ])
        }
    }

    private func ensureDirectory() throws {
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
    }

    private func decode<T: Decodable>(_ type: T.Type, from url: URL) throws -> T {
        try JSONDecoder().decode(type, from: Data(contentsOf: url))
    }

    private func encode<T: Encodable>(_ value: T, to url: URL) throws {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
        var data = try encoder.encode(value)
        data.append(0x0A)
        try data.write(to: url, options: .atomic)
        try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: url.path)
    }
}
