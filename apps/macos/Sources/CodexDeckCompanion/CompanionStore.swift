import AppKit
import Foundation
import ServiceManagement
import UserNotifications

@MainActor
final class CompanionStore: ObservableObject {
    @Published var lifecycle: BridgeLifecycle = .stopped
    @Published var status: ManagedBridgeStatus?
    @Published var devices: [ConnectedDevice] = []
    @Published var logs: [LogEntry] = []
    @Published var approvals: [DesktopApproval] = []
    @Published var inputs: [PendingInput] = []
    @Published var settings = ManagedSettings()
    @Published var registry = ManagedWorkflowRegistry()
    @Published var codexCheck = CodexCheck(authenticated: false)
    @Published var message: String?
    @Published var externalBridgeDetected = false
    @Published var onboardingComplete: Bool
    @Published var signInOutput = ""
    @Published var signingIn = false
    @Published var deviceAuthURL: URL?
    @Published var deviceAuthCode: String?

    let configStore: ConfigStore
    private let controller = BridgeProcessController()
    private var management: ManagementClient?
    private var runtime: CompanionRuntime?
    private var refreshTask: Task<Void, Never>?
    private var authProcess: Process?
    private var restartAttempt = 0
    private var restartWorkItem: DispatchWorkItem?
    private var hasRefreshed = false
    private var lastAttentionCount = 0
    private var lastDeviceIDs = Set<String>()
    private var terminationObserver: NSObjectProtocol?

    init(configStore: ConfigStore = ConfigStore()) {
        self.configStore = configStore
        onboardingComplete = UserDefaults.standard.bool(forKey: "CodexDeck.OnboardingComplete")
        configureController()
        loadConfiguration()
        let bridgeController = controller
        terminationObserver = NotificationCenter.default.addObserver(
            forName: NSApplication.willTerminateNotification,
            object: nil,
            queue: .main
        ) { _ in
            bridgeController.terminateForApplicationExit()
        }
        // Opening the companion should always own and start the bridge after setup.
        // Start at Login only controls whether macOS launches the companion at login.
        if onboardingComplete {
            DispatchQueue.main.async { [weak self] in self?.startBridge() }
        }
    }

    deinit {
        if let terminationObserver { NotificationCenter.default.removeObserver(terminationObserver) }
    }

    var startAtLogin: Bool {
        SMAppService.mainApp.status == .enabled
    }

    var sortedProjects: [ManagedProject] {
        registry.projects.values.sorted { $0.order == $1.order ? $0.label < $1.label : $0.order < $1.order }
    }

    var dataDirectory: URL { configStore.directory }

    func completeOnboarding() {
        onboardingComplete = true
        UserDefaults.standard.set(true, forKey: "CodexDeck.OnboardingComplete")
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { _, _ in }
        startBridge()
    }

    func startBridge() {
        guard lifecycle == .stopped || lifecycle == .failed || externalBridgeDetected else { return }
        guard codexCheck.authenticated else {
            lifecycle = .degraded
            message = "Sign in to Codex before starting the bridge."
            return
        }
        guard let runtime else {
            lifecycle = .failed
            message = "The bundled bridge runtime is missing. Run pnpm build for a development launch."
            return
        }
        externalBridgeDetected = false
        message = nil
        Task {
            if await probeExternalBridge() {
                externalBridgeDetected = true
                lifecycle = .degraded
                message = "A bridge is already using this address. It was left running and attached as view-only."
                return
            }
            do {
                try controller.start(runtime: runtime, configURL: configStore.settingsURL)
            } catch {
                lifecycle = .failed
                message = error.localizedDescription
            }
        }
    }

    func stopBridge() {
        restartWorkItem?.cancel()
        restartWorkItem = nil
        refreshTask?.cancel()
        refreshTask = nil
        management?.disconnect()
        let client = management
        management = nil
        if externalBridgeDetected {
            externalBridgeDetected = false
            lifecycle = .stopped
            status = nil
            message = nil
            return
        }
        controller.stop {
            Task { try? await client?.shutdown() }
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1) { [weak self] in
            guard self?.lifecycle == .stopping else { return }
            self?.lifecycle = .stopped
            self?.status = nil
            self?.devices = []
        }
    }

    func quit() {
        restartWorkItem?.cancel()
        refreshTask?.cancel()
        management?.disconnect()
        controller.terminateForApplicationExit()
        NSApp.terminate(nil)
    }

    func restartBridge() {
        stopBridge()
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { [weak self] in self?.startBridge() }
    }

    func clearFinishedTasks() {
        guard let management else {
            message = "The bridge is not available."
            return
        }
        Task {
            do {
                let cleared = try await management.clearFinishedTasks()
                message = cleared == 0
                    ? "No finished tasks to clear."
                    : "Cleared \(cleared) finished task\(cleared == 1 ? "" : "s")."
                await refresh()
            } catch {
                message = error.localizedDescription
            }
        }
    }

    func saveConfiguration(restart: Bool = false) {
        do {
            try validateConfiguration()
            try configStore.save(settings: settings, workflows: registry)
            message = "Configuration saved"
            if restart, lifecycle == .online || lifecycle == .degraded {
                restartBridge()
            } else if let management {
                Task {
                    do { try await management.reload() }
                    catch { self.message = "Saved, but reload failed: \(error.localizedDescription)" }
                }
            }
        } catch {
            message = error.localizedDescription
        }
    }

    func setStartAtLogin(_ enabled: Bool) {
        do {
            if enabled { try SMAppService.mainApp.register() }
            else { try SMAppService.mainApp.unregister() }
            objectWillChange.send()
        } catch {
            message = "Start at Login could not be changed: \(error.localizedDescription)"
        }
    }

    func chooseCodex() {
        guard let path = CodexLocator.chooseExecutable() else { return }
        settings.codexPath = path
        refreshCodex()
        saveConfiguration(restart: true)
    }

    func refreshCodex() {
        let path = CodexLocator.locate(savedPath: settings.codexPath)
        codexCheck = CodexLocator.check(path: path)
        if let path { settings.codexPath = path }
    }

    func beginDeviceAuth() {
        guard authProcess == nil, let path = codexCheck.path ?? CodexLocator.locate(savedPath: settings.codexPath) else {
            message = "Codex CLI was not found."
            return
        }
        signingIn = true
        signInOutput = "Starting Codex device authentication…\n"
        deviceAuthURL = nil
        deviceAuthCode = nil
        let process = Process()
        let pipe = Pipe()
        process.executableURL = URL(fileURLWithPath: path)
        process.arguments = ["login", "--device-auth"]
        process.standardOutput = pipe
        process.standardError = pipe
        pipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            guard !data.isEmpty, let text = String(data: data, encoding: .utf8) else { return }
            DispatchQueue.main.async {
                guard let self else { return }
                self.signInOutput += TerminalText.clean(text)
                if self.deviceAuthURL == nil, let url = TerminalText.firstURL(in: self.signInOutput) {
                    self.deviceAuthURL = url
                    NSWorkspace.shared.open(url)
                }
                if self.deviceAuthCode == nil {
                    self.deviceAuthCode = TerminalText.deviceCode(in: self.signInOutput)
                }
            }
        }
        process.terminationHandler = { [weak self] _ in
            DispatchQueue.main.async {
                pipe.fileHandleForReading.readabilityHandler = nil
                self?.authProcess = nil
                self?.signingIn = false
                self?.refreshCodex()
                if self?.codexCheck.authenticated == true { self?.signInOutput += "\nSign-in complete.\n" }
            }
        }
        do {
            try process.run()
            authProcess = process
        } catch {
            signingIn = false
            message = error.localizedDescription
        }
    }

    func copyDeviceAuthCode() {
        guard let deviceAuthCode else { return }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(deviceAuthCode, forType: .string)
        message = "Sign-in code copied"
    }

    func addProject() {
        let panel = NSOpenPanel()
        panel.message = "Choose a folder CodexDeck may use"
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        guard panel.runModal() == .OK, let url = panel.url else { return }
        let base = slug(url.lastPathComponent)
        var id = base
        var suffix = 2
        while registry.projects[id] != nil { id = "\(base)-\(suffix)"; suffix += 1 }
        registry.projects[id] = ManagedProject(id: id, label: url.lastPathComponent, cwd: url.path, order: registry.projects.count)
        saveConfiguration()
    }

    func removeProject(id: String) {
        registry.projects.removeValue(forKey: id)
        normalizeOrders()
        saveConfiguration()
    }

    func addWorkflow(projectID: String) {
        guard var project = registry.projects[projectID] else { return }
        var id = "new-workflow"
        var suffix = 2
        while project.workflows[id] != nil { id = "new-workflow-\(suffix)"; suffix += 1 }
        project.workflows[id] = ManagedWorkflow(
            id: id,
            label: "New workflow",
            shortLabel: "New",
            prompt: "Describe what Codex should do in this project.",
            order: project.workflows.count
        )
        registry.projects[projectID] = project
        saveConfiguration()
    }

    func duplicateWorkflow(projectID: String, workflowID: String) {
        guard var project = registry.projects[projectID], var workflow = project.workflows[workflowID] else { return }
        let base = "\(workflowID)-copy"
        var id = base
        var suffix = 2
        while project.workflows[id] != nil { id = "\(base)-\(suffix)"; suffix += 1 }
        workflow.id = id
        workflow.label += " Copy"
        workflow.order = project.workflows.count
        project.workflows[id] = workflow
        registry.projects[projectID] = project
        saveConfiguration()
    }

    func removeWorkflow(projectID: String, workflowID: String) {
        guard var project = registry.projects[projectID] else { return }
        project.workflows.removeValue(forKey: workflowID)
        registry.projects[projectID] = project
        saveConfiguration()
    }

    func respond(input: PendingInput, answers: [String: [String]]) {
        guard let management else { return }
        Task {
            do { try await management.respond(inputID: input.id, answers: answers); await refresh() }
            catch { message = error.localizedDescription }
        }
    }

    func respond(approval: DesktopApproval, decision: String) {
        guard let management else { return }
        Task {
            do { try await management.respond(approvalID: approval.id, decision: decision); await refresh() }
            catch { message = error.localizedDescription }
        }
    }

    func copyDiagnostics() {
        let diagnostics = [
            "CodexDeck Companion",
            "Lifecycle: \(lifecycle.rawValue)",
            "Address: \(status?.address ?? "not listening")",
            "Bridge: \(status?.bridgeVersion ?? "unknown")",
            "Codex: \(codexCheck.version ?? "unknown")",
            "Codex ready: \(status?.codexReady.description ?? "false")",
            "Devices: \(devices.count)",
            "Active tasks: \(status?.activeTasks ?? 0)",
            "Last error: \(status?.lastError ?? message ?? "none")",
        ].joined(separator: "\n")
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(diagnostics, forType: .string)
        message = "Diagnostics copied"
    }

    func openDataDirectory() { NSWorkspace.shared.open(dataDirectory) }
    func openFirmwareGuide() {
        if let bundled = Bundle.main.resourceURL?.appendingPathComponent("Docs/hardware-test-checklist.md"),
           FileManager.default.fileExists(atPath: bundled.path) {
            NSWorkspace.shared.open(bundled)
        } else if let root = RuntimeLocator.repositoryRoot() {
            NSWorkspace.shared.open(root.appendingPathComponent("docs/hardware-test-checklist.md"))
        }
    }

    private func configureController() {
        controller.onLifecycle = { [weak self] state, message in
            DispatchQueue.main.async { self?.lifecycle = state; if let message { self?.message = message } }
        }
        controller.onManagementReady = { [weak self] port, token in self?.connectManagement(port: port, token: token) }
        controller.onUnexpectedExit = { [weak self] status in self?.handleUnexpectedExit(status) }
    }

    private func loadConfiguration() {
        runtime = RuntimeLocator.locate()
        do {
            if !FileManager.default.fileExists(atPath: configStore.settingsURL.path),
               let legacy = configStore.legacyConfiguration(), let runtime {
                try configStore.migrate(node: runtime.node, helper: runtime.migrationHelper, legacy: legacy)
            }
            let located = CodexLocator.locate()
            (settings, registry) = try configStore.loadOrCreate(codexPath: located ?? "codex")
            refreshCodex()
            if let path = codexCheck.path, settings.codexPath != path {
                settings.codexPath = path
                try configStore.save(settings: settings, workflows: registry)
            }
        } catch {
            lifecycle = .failed
            message = "Configuration could not be loaded: \(error.localizedDescription)"
        }
    }

    private func connectManagement(port: Int, token: String) {
        let client = ManagementClient(port: port, token: token)
        management = client
        client.onEvent = { [weak self] _ in Task { @MainActor in await self?.refresh() } }
        client.onDisconnect = { [weak self] error in
            DispatchQueue.main.async {
                if self?.lifecycle == .online { self?.lifecycle = .degraded }
                if let error { self?.message = error.localizedDescription }
            }
        }
        client.connectEvents()
        restartAttempt = 0
        refreshTask?.cancel()
        refreshTask = Task {
            while !Task.isCancelled {
                await refresh()
                try? await Task.sleep(for: .seconds(2))
            }
        }
    }

    private func refresh() async {
        guard let management else { return }
        do {
            async let nextStatus = management.fetchStatus()
            async let nextDevices = management.fetchDevices()
            async let nextLogs = management.fetchLogs()
            async let nextAttention = management.fetchAttention()
            let result = try await (nextStatus, nextDevices, nextLogs, nextAttention)
            let attentionCount = result.3.approvals.count + result.3.inputs.count
            let deviceIDs = Set(result.1.map(\.deviceId))
            if hasRefreshed, settings.notifications.attention, attentionCount > lastAttentionCount {
                notify(title: "CodexDeck needs attention", body: "Codex is waiting for input or approval.")
            }
            if hasRefreshed, settings.notifications.deviceChanges, deviceIDs != lastDeviceIDs {
                notify(title: "Cardputer connection changed", body: "\(deviceIDs.count) Cardputer device\(deviceIDs.count == 1 ? "" : "s") connected.")
            }
            status = result.0
            lifecycle = result.0.lifecycle
            devices = result.1
            logs = result.2
            approvals = result.3.approvals
            inputs = result.3.inputs
            lastAttentionCount = attentionCount
            lastDeviceIDs = deviceIDs
            hasRefreshed = true
        } catch {
            if lifecycle == .online { lifecycle = .degraded }
        }
    }

    private func handleUnexpectedExit(_ status: Int32) {
        management?.disconnect()
        management = nil
        refreshTask?.cancel()
        guard restartAttempt < BridgeProcessController.restartDelays.count else {
            lifecycle = .failed
            message = "Bridge exited repeatedly with status \(status)."
            return
        }
        let delay = BridgeProcessController.restartDelays[restartAttempt]
        restartAttempt += 1
        lifecycle = .starting
        message = "Bridge exited. Restarting in \(Int(delay)) seconds."
        let item = DispatchWorkItem { [weak self] in self?.startBridge() }
        restartWorkItem = item
        DispatchQueue.main.asyncAfter(deadline: .now() + delay, execute: item)
    }

    private func probeExternalBridge() async -> Bool {
        let hosts: [String]
        switch settings.bindMode {
        case "explicit": hosts = [settings.bindHost ?? "127.0.0.1"]
        case "loopback": hosts = ["127.0.0.1"]
        default: hosts = NetworkAddresses.privateIPv4().map(\.address) + ["127.0.0.1"]
        }
        for host in hosts {
            guard let url = URL(string: "http://\(host):\(settings.port)/healthz") else { continue }
            var request = URLRequest(url: url)
            request.timeoutInterval = 0.35
            if let (_, response) = try? await URLSession.shared.data(for: request),
               let http = response as? HTTPURLResponse, http.statusCode == 200 { return true }
        }
        return false
    }

    private func validateConfiguration() throws {
        guard (1...65535).contains(settings.port) else { throw validation("Port must be between 1 and 65535") }
        if settings.bindMode == "explicit", settings.bindHost?.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty != false {
            throw validation("An explicit bind host is required")
        }
        var ids = Set<String>()
        for (id, project) in registry.projects {
            guard ids.insert(id).inserted else { throw validation("Duplicate project ID: \(id)") }
            guard id.range(of: "^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$", options: .regularExpression) != nil else { throw validation("Invalid project ID: \(id)") }
            guard project.label.count <= 48, !project.label.isEmpty else { throw validation("Project labels must contain 1 to 48 characters") }
            var isDirectory: ObjCBool = false
            guard project.cwd.hasPrefix("/"), FileManager.default.fileExists(atPath: project.cwd, isDirectory: &isDirectory), isDirectory.boolValue else { throw validation("Project folder does not exist: \(project.cwd)") }
            for (workflowID, workflow) in project.workflows {
                guard workflowID.range(of: "^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$", options: .regularExpression) != nil else { throw validation("Invalid workflow ID: \(workflowID)") }
                guard !workflow.prompt.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty, workflow.prompt.count <= 8_000 else { throw validation("Workflow prompts must contain 1 to 8,000 characters") }
                guard (1...16).contains(workflow.shortLabel.count) else { throw validation("Cardputer labels must contain 1 to 16 characters") }
            }
        }
    }

    private func normalizeOrders() {
        for (index, project) in sortedProjects.enumerated() { registry.projects[project.id]?.order = index }
    }

    private func slug(_ value: String) -> String {
        let lowered = value.lowercased().replacingOccurrences(of: "[^a-z0-9]+", with: "-", options: .regularExpression)
            .trimmingCharacters(in: CharacterSet(charactersIn: "-"))
        return String((lowered.isEmpty ? "project" : lowered).prefix(64))
    }

    private func validation(_ message: String) -> NSError {
        NSError(domain: "CodexDeck.Configuration", code: 1, userInfo: [NSLocalizedDescriptionKey: message])
    }

    private func notify(title: String, body: String) {
        let content = UNMutableNotificationContent()
        content.title = title
        content.body = body
        content.sound = .default
        UNUserNotificationCenter.current().add(UNNotificationRequest(identifier: UUID().uuidString, content: content, trigger: nil))
    }
}
