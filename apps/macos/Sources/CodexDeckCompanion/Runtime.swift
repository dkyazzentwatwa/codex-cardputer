import Foundation

struct CompanionRuntime: Equatable {
    var node: URL
    var bridgeEntry: URL
    var migrationHelper: URL
}

enum RuntimeLocator {
    static func locate(bundle: Bundle = .main) -> CompanionRuntime? {
        if let resources = bundle.resourceURL {
            let bundled = CompanionRuntime(
                node: resources.appendingPathComponent("Runtime/node"),
                bridgeEntry: resources.appendingPathComponent("Bridge/dist/index.js"),
                migrationHelper: resources.appendingPathComponent("Bridge/dist/management/migrate-config.js")
            )
            if valid(bundled) { return bundled }
        }

        guard let root = repositoryRoot() else { return nil }
        let nodePath = ProcessInfo.processInfo.environment["CODEXDECK_NODE"]
            ?? CodexLocator.run("/bin/zsh", arguments: ["-lic", "command -v node"]).output?
                .trimmingCharacters(in: .whitespacesAndNewlines)
        guard let nodePath else { return nil }
        let development = CompanionRuntime(
            node: URL(fileURLWithPath: nodePath),
            bridgeEntry: root.appendingPathComponent("apps/bridge/dist/index.js"),
            migrationHelper: root.appendingPathComponent("apps/bridge/dist/management/migrate-config.js")
        )
        return valid(development) ? development : nil
    }

    static func repositoryRoot() -> URL? {
        if let explicit = ProcessInfo.processInfo.environment["CODEXDECK_REPO_ROOT"] {
            return URL(fileURLWithPath: explicit)
        }
        var candidate = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
        for _ in 0..<8 {
            if FileManager.default.fileExists(atPath: candidate.appendingPathComponent("pnpm-workspace.yaml").path) {
                return candidate
            }
            candidate.deleteLastPathComponent()
        }
        let conventional = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Documents/GitHub/codex-cardputer")
        return FileManager.default.fileExists(atPath: conventional.appendingPathComponent("pnpm-workspace.yaml").path)
            ? conventional
            : nil
    }

    private static func valid(_ runtime: CompanionRuntime) -> Bool {
        FileManager.default.isExecutableFile(atPath: runtime.node.path)
            && FileManager.default.fileExists(atPath: runtime.bridgeEntry.path)
            && FileManager.default.fileExists(atPath: runtime.migrationHelper.path)
    }
}

final class LineBuffer {
    private var buffer = Data()

    func append(_ data: Data) -> [String] {
        buffer.append(data)
        var lines: [String] = []
        while let newline = buffer.firstIndex(of: 0x0A) {
            let line = buffer.prefix(upTo: newline)
            buffer.removeSubrange(...newline)
            if let text = String(data: line, encoding: .utf8), !text.isEmpty { lines.append(text) }
        }
        return lines
    }
}
