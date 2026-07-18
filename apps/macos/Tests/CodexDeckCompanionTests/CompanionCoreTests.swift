import Foundation
import Testing
@testable import CodexDeckCompanion

@Suite("CodexDeck companion core")
struct CompanionCoreTests {
    @Test("Configuration round trips with secure managed files")
    func configurationRoundTrip() throws {
        let root = FileManager.default.temporaryDirectory
            .appendingPathComponent("codexdeck-swift-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: root) }
        let store = ConfigStore(directory: root)
        var settings = ManagedSettings()
        settings.codexPath = "/opt/homebrew/bin/codex"
        let project = ManagedProject(id: "demo", label: "Demo", cwd: root.path)
        let workflows = ManagedWorkflowRegistry(projects: ["demo": project])

        try store.save(settings: settings, workflows: workflows)
        let loaded = try store.loadOrCreate(codexPath: "codex")
        #expect(loaded.0 == settings)
        #expect(loaded.1.projects["demo"]?.label == "Demo")
        let attributes = try FileManager.default.attributesOfItem(atPath: store.settingsURL.path)
        #expect((attributes[.posixPermissions] as? NSNumber)?.intValue == 0o600)
    }

    @Test("Line buffer preserves split JSON lines")
    func lineBuffer() {
        let buffer = LineBuffer()
        #expect(buffer.append(Data("{\"event\":".utf8)).isEmpty)
        #expect(buffer.append(Data("\"ready\"}\nnext\n".utf8)) == ["{\"event\":\"ready\"}", "next"])
    }

    @Test("Crash recovery backoff is bounded")
    func backoff() {
        #expect(BridgeProcessController.restartDelays == [1, 2, 5, 15])
    }

    @Test("Workflow identifiers survive dictionary encoding")
    func workflowIdentifiers() throws {
        let workflow = ManagedWorkflow(id: "review", label: "Review", shortLabel: "Review", prompt: "Review this project")
        var project = ManagedProject(id: "demo", label: "Demo", cwd: "/tmp")
        project.workflows[workflow.id] = workflow
        let data = try JSONEncoder().encode(ManagedWorkflowRegistry(projects: [project.id: project]))
        let decoded = try JSONDecoder().decode(ManagedWorkflowRegistry.self, from: data)
        #expect(decoded.projects["demo"]?.workflows["review"]?.id == "review")
    }

    @Test("Private LAN detection excludes public and loopback addresses")
    func privateAddresses() {
        #expect(NetworkAddresses.isPrivate("10.0.0.47"))
        #expect(NetworkAddresses.isPrivate("172.20.1.2"))
        #expect(NetworkAddresses.isPrivate("192.168.1.20"))
        #expect(!NetworkAddresses.isPrivate("127.0.0.1"))
        #expect(!NetworkAddresses.isPrivate("8.8.8.8"))
    }

    @Test("Device authentication output is cleaned and parsed")
    func deviceAuthenticationOutput() {
        let output = "\u{001B}[94mhttps://auth.openai.com/codex/device\u{001B}[0m\n\u{001B}[94m8JNG-ZI0FB\u{001B}[0m"
        let clean = TerminalText.clean(output)
        #expect(!clean.contains("\u{001B}"))
        #expect(TerminalText.firstURL(in: clean)?.absoluteString == "https://auth.openai.com/codex/device")
        #expect(TerminalText.deviceCode(in: clean) == "8JNG-ZI0FB")
    }
}
