import SwiftUI

struct CompanionMenuView: View {
    @ObservedObject var store: CompanionStore
    @State private var presentedInput: PendingInput?
    @State private var presentedApproval: DesktopApproval?
    @State private var confirmingClear = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            if !store.onboardingComplete {
                OnboardingView(store: store)
            } else {
                statusHeader
                Divider()
                metrics
                if !store.inputs.isEmpty || !store.approvals.isEmpty { attention }
                if let message = store.message {
                    Text(message).font(.caption).foregroundStyle(store.lifecycle == .failed ? .red : .secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                Divider()
                controls
                footer
            }
        }
        .padding(14)
        .frame(width: 360)
        .sheet(item: $presentedInput) { InputResponseView(store: store, input: $0) }
        .sheet(item: $presentedApproval) { ApprovalResponseView(store: store, item: $0) }
        .confirmationDialog(
            "Clear finished task history?",
            isPresented: $confirmingClear,
            titleVisibility: .visible
        ) {
            Button("Clear \(store.status?.clearableTasks ?? 0) Finished Tasks", role: .destructive) {
                store.clearFinishedTasks()
            }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("Active, waiting, and resumable stale tasks will remain available. Codex threads are not deleted.")
        }
    }

    private var statusHeader: some View {
        HStack(spacing: 10) {
            DeckMark(status: store.lifecycle.color).frame(width: 38, height: 38)
            VStack(alignment: .leading, spacing: 2) {
                Text("CodexDeck").font(.headline)
                Text(store.lifecycle.label).foregroundStyle(store.lifecycle.color).font(.caption.weight(.semibold))
            }
            Spacer()
            if store.lifecycle == .starting || store.lifecycle == .stopping { ProgressView().controlSize(.small) }
        }
    }

    private var metrics: some View {
        VStack(spacing: 7) {
            MetricRow(label: "Bridge", value: store.status?.address ?? "Not listening")
            MetricRow(label: "Codex", value: store.status?.codexReady == true ? "Ready" : (store.codexCheck.authenticated ? "Connecting" : "Sign in required"))
            MetricRow(label: "Cardputers", value: "\(store.devices.count)")
            MetricRow(label: "Active tasks", value: "\(store.status?.activeTasks ?? 0)")
            MetricRow(label: "Task history", value: "\(store.status?.totalTasks ?? 0)")
        }
    }

    private var attention: some View {
        VStack(alignment: .leading, spacing: 7) {
            Text("NEEDS ATTENTION").font(.caption2.weight(.bold)).foregroundStyle(.orange)
            ForEach(store.inputs.prefix(2)) { input in
                Button { presentedInput = input } label: {
                    Label(input.params.questions.first?.question ?? "Codex needs input", systemImage: "questionmark.bubble")
                        .lineLimit(2).frame(maxWidth: .infinity, alignment: .leading)
                }.buttonStyle(.bordered)
            }
            ForEach(store.approvals.prefix(2)) { approval in
                Button { presentedApproval = approval } label: {
                    Label(approval.approval.title, systemImage: approval.approval.risk == "high" ? "exclamationmark.triangle.fill" : "checkmark.shield")
                        .lineLimit(1).frame(maxWidth: .infinity, alignment: .leading)
                }.buttonStyle(.bordered)
            }
        }
    }

    private var controls: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                if store.lifecycle == .stopped || store.lifecycle == .failed {
                    Button("Start Bridge", systemImage: "play.fill") { store.startBridge() }.buttonStyle(.borderedProminent)
                } else {
                    Button("Stop", systemImage: "stop.fill") { store.stopBridge() }
                    Button("Restart", systemImage: "arrow.clockwise") { store.restartBridge() }
                }
                Spacer()
                Button { openCompanionSettings() } label: { Image(systemName: "gearshape") }
                    .help("Open Settings")
            }
            if let count = store.status?.clearableTasks, count > 0 {
                Button("Clear Finished Tasks (\(count))", systemImage: "trash", role: .destructive) {
                    confirmingClear = true
                }
                .buttonStyle(.bordered)
            }
        }
    }

    private var footer: some View {
        HStack {
            Button("Logs") { openCompanionSettings() }.buttonStyle(.link)
            Spacer()
            Button("Quit") { store.quit() }
                .buttonStyle(.link)
        }.font(.caption)
    }
}

struct MetricRow: View {
    var label: String
    var value: String
    var body: some View {
        HStack { Text(label).foregroundStyle(.secondary); Spacer(); Text(value).lineLimit(1).truncationMode(.middle) }
            .font(.caption)
    }
}

struct OnboardingView: View {
    @ObservedObject var store: CompanionStore

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Label("CodexDeck Setup", systemImage: "rectangle.stack.badge.plus").font(.title3.bold())
            Text("Your Cardputer can start and monitor allowlisted Codex workflows on this trusted LAN.")
                .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Label("The device connection is unauthenticated. Do not expose port \(store.settings.port) through tunnels or port forwarding.", systemImage: "exclamationmark.shield")
                .font(.caption).foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true)
            Divider()
            Label(store.codexCheck.version ?? "Codex CLI not found", systemImage: store.codexCheck.path == nil ? "xmark.circle" : "checkmark.circle")
            Label(store.codexCheck.authenticated ? "Signed in to Codex" : "Codex sign-in required", systemImage: store.codexCheck.authenticated ? "person.crop.circle.badge.checkmark" : "person.crop.circle.badge.exclamationmark")
            HStack {
                if !store.codexCheck.authenticated { Button("Sign In") { store.beginDeviceAuth() }.disabled(store.signingIn) }
                Button("Choose Codex…") { store.chooseCodex() }
                Button("Recheck") { store.refreshCodex() }
            }
            if store.signingIn || !store.signInOutput.isEmpty {
                if let url = store.deviceAuthURL {
                    HStack {
                        Link("Open Sign-In Page", destination: url)
                        if let code = store.deviceAuthCode {
                            Text(code).font(.system(.body, design: .monospaced).bold()).textSelection(.enabled)
                            Button("Copy Code") { store.copyDeviceAuthCode() }
                        }
                    }
                }
                ScrollView { Text(store.signInOutput).font(.system(.caption, design: .monospaced)).textSelection(.enabled).frame(maxWidth: .infinity, alignment: .leading) }
                    .frame(height: 90).padding(6).background(.quaternary, in: RoundedRectangle(cornerRadius: 6))
            }
            HStack {
                Label("\(store.registry.projects.count) projects available", systemImage: "folder")
                Spacer()
                Button("Add Project…") { store.addProject() }
            }
            Button("Finish Setup and Start Bridge") { store.completeOnboarding() }
                .buttonStyle(.borderedProminent)
                .disabled(!store.codexCheck.authenticated || store.registry.projects.isEmpty)
                .frame(maxWidth: .infinity, alignment: .trailing)
        }
    }
}

struct SettingsRootView: View {
    @ObservedObject var store: CompanionStore

    var body: some View {
        TabView {
            GeneralSettingsView(store: store).tabItem { Label("General", systemImage: "gearshape") }
            ProjectsSettingsView(store: store).tabItem { Label("Projects", systemImage: "folder") }
            DevicesSettingsView(store: store).tabItem { Label("Devices", systemImage: "rectangle.connected.to.line.below") }
            DiagnosticsView(store: store).tabItem { Label("Diagnostics", systemImage: "waveform.path.ecg") }
        }
        .frame(minWidth: 760, minHeight: 520)
    }
}

struct GeneralSettingsView: View {
    @ObservedObject var store: CompanionStore

    var body: some View {
        Form {
            Section("Bridge") {
                Toggle("Start bridge when the companion opens", isOn: $store.settings.startOnLaunch)
                Toggle("Start companion at login", isOn: Binding(get: { store.startAtLogin }, set: store.setStartAtLogin))
                TextField("Port", value: $store.settings.port, format: .number)
                Picker("Network binding", selection: $store.settings.bindMode) {
                    Text("Automatic private LAN").tag("auto_private")
                    Text("Loopback only").tag("loopback")
                    Text("Explicit host").tag("explicit")
                }
                if store.settings.bindMode == "explicit" { TextField("Bind host", text: Binding(get: { store.settings.bindHost ?? "" }, set: { store.settings.bindHost = $0 })) }
            }
            Section("Codex CLI") {
                TextField("Executable", text: $store.settings.codexPath)
                LabeledContent("Status", value: store.codexCheck.authenticated ? "Signed in" : "Needs attention")
                LabeledContent("Version", value: store.codexCheck.version ?? "Unknown")
                HStack { Button("Choose…") { store.chooseCodex() }; Button("Recheck") { store.refreshCodex() } }
            }
            Section("Notifications") {
                Toggle("Input and approval attention", isOn: $store.settings.notifications.attention)
                Toggle("Device connection changes", isOn: $store.settings.notifications.deviceChanges)
            }
            HStack {
                Button("Save and Restart") { store.saveConfiguration(restart: true) }.buttonStyle(.borderedProminent)
                Button("Open Data Folder") { store.openDataDirectory() }
                Spacer()
                Text(store.message ?? "").foregroundStyle(.secondary).font(.caption)
            }
        }.padding(22)
    }
}

struct ProjectsSettingsView: View {
    @ObservedObject var store: CompanionStore
    @State private var selectedID: String?

    var body: some View {
        NavigationSplitView {
            List(selection: $selectedID) {
                ForEach(store.sortedProjects) { project in
                    HStack {
                        Circle().fill(project.enabled ? Color.cyan : Color.secondary).frame(width: 7, height: 7)
                        Text(project.label)
                    }.tag(project.id)
                }
            }
            .safeAreaInset(edge: .bottom) {
                HStack {
                    Button { store.addProject() } label: { Image(systemName: "plus") }
                    Button { if let selectedID { store.removeProject(id: selectedID); self.selectedID = nil } } label: { Image(systemName: "minus") }.disabled(selectedID == nil)
                    Spacer()
                }.padding(8).background(.bar)
            }
            .navigationSplitViewColumnWidth(min: 180, ideal: 210)
        } detail: {
            if let selectedID, store.registry.projects[selectedID] != nil {
                ProjectEditor(store: store, projectID: selectedID)
            } else {
                EmptyStateView(title: "Select a Project", symbol: "folder", detail: "Projects and Cardputer workflows are configured here.")
            }
        }
        .onAppear { if selectedID == nil { selectedID = store.sortedProjects.first?.id } }
    }
}

struct ProjectEditor: View {
    @ObservedObject var store: CompanionStore
    var projectID: String

    private var project: Binding<ManagedProject> {
        Binding(get: { store.registry.projects[projectID]! }, set: { store.registry.projects[projectID] = $0 })
    }

    var body: some View {
        Form {
            Section("Project") {
                Toggle("Available on Cardputer", isOn: project.enabled)
                TextField("Name", text: project.label)
                LabeledContent("ID", value: projectID)
                TextField("Folder", text: project.cwd)
                TextField("Model override", text: Binding(get: { project.wrappedValue.model ?? "" }, set: { project.wrappedValue.model = $0.isEmpty ? nil : $0 }))
                Picker("Approval policy", selection: project.approvalPolicy) {
                    Text("On request").tag("on-request"); Text("Untrusted").tag("untrusted"); Text("Never").tag("never")
                }
                Picker("Sandbox", selection: project.sandbox) {
                    Text("Read only").tag("read-only"); Text("Workspace write").tag("workspace-write"); Text("Full access").tag("danger-full-access")
                }
            }
            Section("Workflows") {
                ForEach(project.wrappedValue.workflows.values.sorted(by: { $0.order < $1.order })) { workflow in
                    WorkflowEditor(store: store, projectID: projectID, workflowID: workflow.id)
                }
                Button("Add Workflow", systemImage: "plus") { store.addWorkflow(projectID: projectID) }
            }
            HStack { Button("Save and Reload") { store.saveConfiguration() }.buttonStyle(.borderedProminent); Spacer() }
        }.formStyle(.grouped)
    }
}

struct WorkflowEditor: View {
    @ObservedObject var store: CompanionStore
    var projectID: String
    var workflowID: String
    @State private var expanded = false

    private var workflow: Binding<ManagedWorkflow> {
        Binding(
            get: { store.registry.projects[projectID]!.workflows[workflowID]! },
            set: { store.registry.projects[projectID]!.workflows[workflowID] = $0 }
        )
    }

    var body: some View {
        DisclosureGroup(isExpanded: $expanded) {
            VStack(alignment: .leading, spacing: 8) {
                TextField("Name", text: workflow.label)
                TextField("Cardputer label", text: workflow.shortLabel)
                TextField("Optional skill", text: Binding(get: { workflow.wrappedValue.skill ?? "" }, set: { workflow.wrappedValue.skill = $0.isEmpty ? nil : $0 }))
                Text("Prompt").font(.caption).foregroundStyle(.secondary)
                TextEditor(text: workflow.prompt).font(.system(.body, design: .monospaced)).frame(minHeight: 100).border(.quaternary)
                HStack {
                    Button("Duplicate") { store.duplicateWorkflow(projectID: projectID, workflowID: workflowID) }
                    Button("Delete", role: .destructive) { store.removeWorkflow(projectID: projectID, workflowID: workflowID) }
                }
            }.padding(.top, 8)
        } label: {
            Toggle(isOn: workflow.enabled) { Text(workflow.wrappedValue.label).fontWeight(.medium) }
        }
    }
}

struct DevicesSettingsView: View {
    @ObservedObject var store: CompanionStore

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            if store.devices.isEmpty {
                EmptyStateView(title: "No Cardputer Connected", symbol: "rectangle.slash", detail: "Connect the Cardputer to the same trusted LAN. The bridge advertises itself over mDNS.")
            } else {
                List(store.devices) { device in
                    VStack(alignment: .leading, spacing: 5) {
                        HStack { Text(device.deviceName).font(.headline); Spacer(); Text("Connected").foregroundStyle(.cyan) }
                        LabeledContent("Device ID", value: device.deviceId)
                        LabeledContent("Firmware", value: device.firmwareVersion)
                        LabeledContent("Protocol", value: device.protocolVersion)
                        LabeledContent("Last seen", value: device.lastSeen)
                    }.padding(.vertical, 7)
                }
            }
            HStack { Button("Open Firmware Guide") { store.openFirmwareGuide() }; Spacer(); Text("Firmware installation uses Arduino CLI only.").font(.caption).foregroundStyle(.secondary) }
        }.padding(20)
    }
}

struct EmptyStateView: View {
    var title: String
    var symbol: String
    var detail: String

    var body: some View {
        VStack(spacing: 10) {
            Image(systemName: symbol).font(.system(size: 32)).foregroundStyle(.secondary)
            Text(title).font(.headline)
            Text(detail).foregroundStyle(.secondary).multilineTextAlignment(.center).frame(maxWidth: 360)
        }.frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

struct DiagnosticsView: View {
    @ObservedObject var store: CompanionStore

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                VStack(alignment: .leading) {
                    Text("Bridge \(store.status?.bridgeVersion ?? "not running")").font(.headline)
                    Text(store.status?.address ?? "No listening address").foregroundStyle(.secondary)
                }
                Spacer()
                Button("Copy Diagnostics") { store.copyDiagnostics() }
                Button("Open Data Folder") { store.openDataDirectory() }
            }
            Divider()
            Text("Recent bridge logs").font(.headline)
            List(store.logs) { entry in
                HStack(alignment: .firstTextBaseline) {
                    Text(entry.timestamp).foregroundStyle(.secondary)
                    Text(entry.level.uppercased()).foregroundStyle(entry.level == "error" ? .red : entry.level == "warn" ? .orange : .secondary).frame(width: 48, alignment: .leading)
                    Text("\(entry.component).\(entry.event)").textSelection(.enabled)
                }.font(.system(.caption, design: .monospaced))
            }
            if let error = store.status?.lastError ?? store.message { Label(error, systemImage: "exclamationmark.triangle").foregroundStyle(.orange) }
        }.padding(20)
    }
}

struct InputResponseView: View {
    @ObservedObject var store: CompanionStore
    var input: PendingInput
    @Environment(\.dismiss) private var dismiss
    @State private var answers: [String: String] = [:]

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Label("Codex Needs Input", systemImage: "questionmark.bubble").font(.title2.bold())
            ForEach(input.params.questions) { question in
                VStack(alignment: .leading, spacing: 6) {
                    if let header = question.header { Text(header.uppercased()).font(.caption.bold()).foregroundStyle(.cyan) }
                    Text(question.question)
                    if let options = question.options, !options.isEmpty {
                        Picker("Answer", selection: Binding(get: { answers[question.id] ?? options[0].label }, set: { answers[question.id] = $0 })) {
                            ForEach(options) { option in Text(option.label).tag(option.label) }
                        }.labelsHidden()
                    } else if question.isSecret == true {
                        SecureField("Answer", text: Binding(get: { answers[question.id] ?? "" }, set: { answers[question.id] = $0 }))
                    } else {
                        TextField("Answer", text: Binding(get: { answers[question.id] ?? "" }, set: { answers[question.id] = $0 }))
                    }
                }
            }
            HStack {
                Spacer()
                Button("Cancel", role: .cancel) { dismiss() }
                Button("Send Answer") {
                    var result: [String: [String]] = [:]
                    for question in input.params.questions {
                        let fallback = question.options?.first?.label ?? ""
                        let value = answers[question.id] ?? fallback
                        result[question.id] = value.isEmpty ? [] : [value]
                    }
                    store.respond(input: input, answers: result)
                    dismiss()
                }.buttonStyle(.borderedProminent)
            }
        }.padding(22).frame(width: 460)
    }
}

struct ApprovalResponseView: View {
    @ObservedObject var store: CompanionStore
    var item: DesktopApproval
    @Environment(\.dismiss) private var dismiss
    @State private var confirmHighRisk = false

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Label(item.approval.title, systemImage: item.approval.risk == "high" ? "exclamationmark.triangle.fill" : "checkmark.shield").font(.title2.bold()).foregroundStyle(item.approval.risk == "high" ? .red : .primary)
            Text(item.approval.summary)
            if let command = item.approval.command { Text(command).font(.system(.body, design: .monospaced)).padding(8).frame(maxWidth: .infinity, alignment: .leading).background(.quaternary, in: RoundedRectangle(cornerRadius: 6)) }
            if let reason = item.approval.riskReason { Label(reason, systemImage: "exclamationmark.shield").foregroundStyle(.orange) }
            HStack {
                Button("Cancel Task", role: .destructive) { decide("cancel") }
                Button("Decline") { decide("decline") }
                Spacer()
                if item.desktopDecisions.contains("acceptForSession") { Button("Allow for Session") { accept("acceptForSession") } }
                Button("Accept") { accept("accept") }.buttonStyle(.borderedProminent)
            }
        }
        .padding(22).frame(width: 500)
        .confirmationDialog("Confirm high-risk approval", isPresented: $confirmHighRisk, titleVisibility: .visible) {
            Button("Accept High-Risk Action", role: .destructive) { decide("accept") }
            Button("Keep Waiting", role: .cancel) {}
        } message: { Text(item.approval.riskReason ?? item.approval.summary) }
    }

    private func accept(_ decision: String) {
        if item.approval.risk == "high" { confirmHighRisk = true }
        else { decide(decision) }
    }

    private func decide(_ decision: String) {
        store.respond(approval: item, decision: decision)
        dismiss()
    }
}
