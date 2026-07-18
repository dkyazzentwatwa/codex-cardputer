import Foundation
import SwiftUI

enum BridgeLifecycle: String, Codable, CaseIterable {
    case stopped
    case starting
    case online
    case degraded
    case stopping
    case failed

    var label: String {
        switch self {
        case .stopped: "Stopped"
        case .starting: "Starting"
        case .online: "Online"
        case .degraded: "Needs attention"
        case .stopping: "Stopping"
        case .failed: "Failed"
        }
    }

    var color: Color {
        switch self {
        case .stopped: .secondary
        case .starting, .degraded, .stopping: .orange
        case .online: Color(red: 0.0, green: 0.82, blue: 0.92)
        case .failed: .red
        }
    }

    var symbol: String {
        switch self {
        case .stopped: "rectangle.stack.badge.minus"
        case .starting, .stopping: "arrow.triangle.2.circlepath"
        case .online: "rectangle.stack.badge.checkmark"
        case .degraded: "exclamationmark.triangle"
        case .failed: "xmark.octagon"
        }
    }
}

struct NotificationSettings: Codable, Equatable {
    var attention = true
    var deviceChanges = false
}

struct ManagedSettings: Codable, Equatable {
    var version = 1
    var port = 8765
    var bindMode = "auto_private"
    var bindHost: String?
    var codexPath = "codex"
    var workflowsFile = "workflows.json"
    var stateFile: String? = "state.json"
    var logsDirectory: String? = "logs"
    var startOnLaunch = true
    var notifications = NotificationSettings()
    var prettyLogs = false
}

struct ManagedWorkflow: Codable, Equatable, Identifiable {
    var id: String
    var label: String
    var shortLabel: String
    var prompt: String
    var skill: String?
    var enabled = true
    var order = 0

    enum CodingKeys: String, CodingKey {
        case label, shortLabel, prompt, skill, enabled, order
    }

    init(
        id: String,
        label: String,
        shortLabel: String,
        prompt: String,
        skill: String? = nil,
        enabled: Bool = true,
        order: Int = 0
    ) {
        self.id = id
        self.label = label
        self.shortLabel = shortLabel
        self.prompt = prompt
        self.skill = skill
        self.enabled = enabled
        self.order = order
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = ""
        label = try container.decode(String.self, forKey: .label)
        shortLabel = try container.decode(String.self, forKey: .shortLabel)
        prompt = try container.decode(String.self, forKey: .prompt)
        skill = try container.decodeIfPresent(String.self, forKey: .skill)
        enabled = try container.decodeIfPresent(Bool.self, forKey: .enabled) ?? true
        order = try container.decodeIfPresent(Int.self, forKey: .order) ?? 0
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(label, forKey: .label)
        try container.encode(shortLabel, forKey: .shortLabel)
        try container.encode(prompt, forKey: .prompt)
        try container.encodeIfPresent(skill, forKey: .skill)
        try container.encode(enabled, forKey: .enabled)
        try container.encode(order, forKey: .order)
    }
}

struct ManagedProject: Codable, Equatable, Identifiable {
    var id: String
    var label: String
    var cwd: String
    var enabled = true
    var order = 0
    var model: String?
    var approvalPolicy = "on-request"
    var sandbox = "workspace-write"
    var workflows: [String: ManagedWorkflow] = [:]

    enum CodingKeys: String, CodingKey {
        case label, cwd, enabled, order, model, approvalPolicy, sandbox, workflows
    }

    init(id: String, label: String, cwd: String, order: Int = 0) {
        self.id = id
        self.label = label
        self.cwd = cwd
        self.order = order
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = ""
        label = try container.decode(String.self, forKey: .label)
        cwd = try container.decode(String.self, forKey: .cwd)
        enabled = try container.decodeIfPresent(Bool.self, forKey: .enabled) ?? true
        order = try container.decodeIfPresent(Int.self, forKey: .order) ?? 0
        model = try container.decodeIfPresent(String.self, forKey: .model)
        approvalPolicy = try container.decodeIfPresent(String.self, forKey: .approvalPolicy) ?? "on-request"
        sandbox = try container.decodeIfPresent(String.self, forKey: .sandbox) ?? "workspace-write"
        let decoded = try container.decodeIfPresent([String: ManagedWorkflow].self, forKey: .workflows) ?? [:]
        workflows = decoded.mapValues { $0 }
        for key in workflows.keys { workflows[key]?.id = key }
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(label, forKey: .label)
        try container.encode(cwd, forKey: .cwd)
        try container.encode(enabled, forKey: .enabled)
        try container.encode(order, forKey: .order)
        try container.encodeIfPresent(model, forKey: .model)
        try container.encode(approvalPolicy, forKey: .approvalPolicy)
        try container.encode(sandbox, forKey: .sandbox)
        try container.encode(workflows, forKey: .workflows)
    }
}

struct ManagedWorkflowRegistry: Codable, Equatable {
    var version = 1
    var projects: [String: ManagedProject] = [:]

    init(version: Int = 1, projects: [String: ManagedProject] = [:]) {
        self.version = version
        self.projects = projects
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        version = try container.decodeIfPresent(Int.self, forKey: .version) ?? 1
        projects = try container.decode([String: ManagedProject].self, forKey: .projects)
        for key in projects.keys { projects[key]?.id = key }
    }
}

struct ManagedBridgeStatus: Codable, Equatable {
    var version: Int
    var lifecycle: BridgeLifecycle
    var bridgeVersion: String
    var testedCodexVersion: String
    var address: String?
    var codexReady: Bool
    var connectedDevices: Int
    var activeTasks: Int
    var totalTasks: Int
    var clearableTasks: Int
    var pendingAttention: Int
    var uptimeSeconds: Int
    var lastError: String?
}

struct ConnectedDevice: Codable, Equatable, Identifiable {
    var deviceId: String
    var deviceName: String
    var hardware: String
    var firmwareVersion: String
    var protocolVersion: String
    var capabilities: [String]
    var connectedAt: String
    var lastSeen: String

    var id: String { deviceId }

    enum CodingKeys: String, CodingKey {
        case deviceId, deviceName, hardware, firmwareVersion, capabilities, connectedAt, lastSeen
        case protocolVersion = "protocol"
    }
}

struct LogEntry: Codable, Equatable, Identifiable {
    var timestamp: String
    var level: String
    var component: String
    var event: String
    var fields: [String: JSONValue]
    var id: String { "\(timestamp)-\(component)-\(event)" }
}

enum JSONValue: Codable, Equatable {
    case string(String), number(Double), bool(Bool), object([String: JSONValue]), array([JSONValue]), null

    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        if container.decodeNil() { self = .null }
        else if let value = try? container.decode(Bool.self) { self = .bool(value) }
        else if let value = try? container.decode(Double.self) { self = .number(value) }
        else if let value = try? container.decode(String.self) { self = .string(value) }
        else if let value = try? container.decode([String: JSONValue].self) { self = .object(value) }
        else { self = .array(try container.decode([JSONValue].self)) }
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        switch self {
        case let .string(value): try container.encode(value)
        case let .number(value): try container.encode(value)
        case let .bool(value): try container.encode(value)
        case let .object(value): try container.encode(value)
        case let .array(value): try container.encode(value)
        case .null: try container.encodeNil()
        }
    }
}

struct InputOption: Codable, Equatable, Identifiable {
    var label: String
    var description: String
    var id: String { label }
}

struct InputQuestion: Codable, Equatable, Identifiable {
    var header: String?
    var id: String
    var question: String
    var isOther: Bool?
    var isSecret: Bool?
    var options: [InputOption]?
}

struct PendingInput: Codable, Equatable, Identifiable {
    struct Parameters: Codable, Equatable {
        var questions: [InputQuestion]
    }
    var id: String
    var taskId: String
    var createdAt: String
    var params: Parameters
}

struct ApprovalSummary: Codable, Equatable, Identifiable {
    var id: String
    var taskId: String
    var type: String
    var title: String
    var summary: String
    var command: String?
    var cwd: String?
    var risk: String
    var riskReason: String?
    var createdAt: String
    var allowedDecisions: [String]
}

struct DesktopApproval: Codable, Equatable, Identifiable {
    var approval: ApprovalSummary
    var method: String
    var desktopDecisions: [String]
    var id: String { approval.id }
}
