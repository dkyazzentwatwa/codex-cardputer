import { randomUUID } from "node:crypto";
import { EventEmitter } from "node:events";
import path from "node:path";

import type { ApprovalDecision, ApprovalRequest } from "@codexdeck/protocol";

import type { AppServerServerRequest } from "../codex/types.js";
import { sanitizeSummary } from "../tasks/summary-builder.js";
import type { TaskRegistry } from "../tasks/task-registry.js";
import { classifyRisk } from "./risk-classifier.js";

interface PendingApproval {
  approval: ApprovalRequest;
  request: AppServerServerRequest;
}

function record(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === "object" ? (value as Record<string, unknown>) : {};
}

export class ApprovalService extends EventEmitter {
  private readonly pending = new Map<string, PendingApproval>();

  constructor(private readonly tasks: TaskRegistry) {
    super();
  }

  open(request: AppServerServerRequest): ApprovalRequest | undefined {
    const params = record(request.params);
    const threadId = typeof params.threadId === "string" ? params.threadId : undefined;
    const task = threadId ? this.tasks.byThread(threadId) : undefined;
    if (!task) {
      request.reject(-32602, "Approval does not belong to a bridge-managed task");
      return undefined;
    }
    const command = typeof params.command === "string" ? params.command : undefined;
    const cwd = typeof params.cwd === "string" ? params.cwd : undefined;
    const reason = typeof params.reason === "string" ? params.reason : undefined;
    const type = request.method.includes("commandExecution")
      ? "command"
      : request.method.includes("fileChange")
        ? "file_change"
        : request.method.includes("permissions")
          ? "permissions"
          : "network";
    const risk = classifyRisk({
      ...(command ? { command } : {}),
      ...(cwd ? { cwd } : {}),
      ...(reason ? { reason } : {}),
    });
    const id = typeof params.approvalId === "string" ? params.approvalId : randomUUID();
    const summary = sanitizeSummary(reason ?? command ?? `${type.replace("_", " ")} approval requested`, 160, cwd);
    const approval: ApprovalRequest = {
      id,
      taskId: task.id,
      type,
      title: type === "file_change" ? "File change approval" : "Command approval",
      summary,
      ...(command ? { command: sanitizeSummary(command, 512, cwd) } : {}),
      ...(cwd ? { cwd: path.basename(cwd) } : {}),
      risk: risk.risk,
      ...(risk.reason ? { riskReason: risk.reason } : {}),
      createdAt: new Date().toISOString(),
      allowedDecisions: ["accept", "decline", "cancel"],
    };
    this.pending.set(id, { approval, request });
    this.tasks.attachApproval(task.id, id, summary);
    this.emit("open", approval);
    return approval;
  }

  list(): ApprovalRequest[] {
    return [...this.pending.values()].map(({ approval }) => approval);
  }

  respond(approvalId: string, decision: ApprovalDecision): ApprovalRequest {
    const pending = this.pending.get(approvalId);
    if (!pending) throw new Error(`Approval not found: ${approvalId}`);
    if (!pending.approval.allowedDecisions.includes(decision)) throw new Error(`Decision not allowed: ${decision}`);
    pending.request.respond({ decision });
    this.pending.delete(approvalId);
    const task = this.tasks.require(pending.approval.taskId);
    if (task.status === "waiting_approval") {
      this.tasks.transition(task.id, "running", decision === "accept" ? "Approval accepted" : "Approval declined");
    }
    this.emit("resolved", { approvalId, decision });
    return pending.approval;
  }
}
