import AppKit
import Foundation

struct CodexCheck: Equatable {
    var path: String?
    var version: String?
    var authenticated: Bool
    var error: String?
}

enum CodexLocator {
    static func locate(savedPath: String? = nil) -> String? {
        let manager = FileManager.default
        var candidates: [String] = []
        if let savedPath, savedPath != "codex" { candidates.append(savedPath) }
        candidates += ["/opt/homebrew/bin/codex", "/usr/local/bin/codex"]
        let nvm = manager.homeDirectoryForCurrentUser.appendingPathComponent(".nvm/versions/node")
        if let versions = try? manager.contentsOfDirectory(at: nvm, includingPropertiesForKeys: nil) {
            candidates += versions.sorted { $0.lastPathComponent > $1.lastPathComponent }
                .map { $0.appendingPathComponent("bin/codex").path }
        }
        if let shellPath = run("/bin/zsh", arguments: ["-lic", "command -v codex"]).output?
            .split(separator: "\n").last.map(String.init) {
            candidates.append(shellPath)
        }
        return candidates.first(where: manager.isExecutableFile(atPath:))
    }

    static func check(path: String?) -> CodexCheck {
        guard let path else { return CodexCheck(authenticated: false, error: "Codex CLI was not found") }
        let version = run(path, arguments: ["--version"])
        guard version.status == 0 else {
            return CodexCheck(path: path, authenticated: false, error: version.error ?? "Codex could not run")
        }
        let login = run(path, arguments: ["login", "status"])
        let loginMessage = [login.output, login.error]
            .compactMap { $0 }
            .joined(separator: "\n")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return CodexCheck(
            path: path,
            version: version.output?.trimmingCharacters(in: .whitespacesAndNewlines),
            authenticated: login.status == 0,
            error: login.status == 0 ? nil : (loginMessage.isEmpty ? "Codex is not signed in" : loginMessage)
        )
    }

    static func chooseExecutable() -> String? {
        let panel = NSOpenPanel()
        panel.message = "Choose the Codex CLI executable"
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        return panel.runModal() == .OK ? panel.url?.path : nil
    }

    static func run(_ executable: String, arguments: [String]) -> (status: Int32, output: String?, error: String?) {
        let process = Process()
        let output = Pipe()
        let errors = Pipe()
        process.executableURL = URL(fileURLWithPath: executable)
        process.arguments = arguments
        process.standardOutput = output
        process.standardError = errors
        do {
            try process.run()
            process.waitUntilExit()
            return (
                process.terminationStatus,
                String(data: output.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8),
                String(data: errors.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8)
            )
        } catch {
            return (-1, nil, error.localizedDescription)
        }
    }
}
