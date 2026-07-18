import { randomUUID } from "node:crypto";
import { EventEmitter } from "node:events";
import path from "node:path";
import { z } from "zod";

import type { ApprovalDecision, ApprovalRequest } from "@codexdeck/protocol";

import type { AppServerServerRequest } from "../codex/types.js";
import { sanitizeSummary } from "../tasks/summary-builder.js";
import type { TaskRegistry } from "../tasks/task-registry.js";
import { classifyRisk } from "./risk-classifier.js";

interface PendingApproval {
  approval: ApprovalRequest;
  request: AppServerServerRequest;
}

const desktopDecisionSchema = z.union([
  z.enum(["accept", "acceptForSession", "decline", "cancel"]),
  z
    .object({
      acceptWithExecpolicyAmendment: z
        .object({ execpolicy_amendment: z.array(z.string().min(1)).min(1) })
        .strict(),
    })
    .strict(),
  z
    .object({
      applyNetworkPolicyAmendment: z
        .object({
          network_policy_amendment: z
            .object({
              action: z.enum(["allow", "deny"]),
              host: z.string().min(1),
            })
            .strict(),
        })
        .strict(),
    })
    .strict(),
]);

const desktopResponseSchema = z.union([
  z.object({ decision: desktopDecisionSchema }).strict(),
  z
    .object({
      permissions: z.record(z.string(), z.unknown()),
      scope: z.enum(["turn", "session"]).optional(),
      strictAutoReview: z.boolean().nullable().optional(),
    })
    .strict(),
]);

export type DesktopApprovalResponse = z.infer<typeof desktopResponseSchema>;

export interface DesktopApproval {
  approval: ApprovalRequest;
  method: string;
  desktopDecisions: Array<"accept" | "acceptForSession" | "decline" | "cancel">;
}

function record(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === "object"
    ? (value as Record<string, unknown>)
    : {};
}

export class ApprovalService extends EventEmitter {
  private readonly pending = new Map<string, PendingApproval>();

  constructor(private readonly tasks: TaskRegistry) {
    super();
  }

  open(request: AppServerServerRequest): ApprovalRequest | undefined {
    const params = record(request.params);
    const threadId =
      typeof params.threadId === "string" ? params.threadId : undefined;
    const task = threadId ? this.tasks.byThread(threadId) : undefined;
    if (!task) {
      request.reject(
        -32602,
        "Approval does not belong to a bridge-managed task",
      );
      return undefined;
    }
    const command =
      typeof params.command === "string" ? params.command : undefined;
    const cwd = typeof params.cwd === "string" ? params.cwd : undefined;
    const reason =
      typeof params.reason === "string" ? params.reason : undefined;
    const grantRoot =
      typeof params.grantRoot === "string" ? params.grantRoot : undefined;
    const permissions = params.permissions;
    const type = params.networkApprovalContext
      ? "network"
      : request.method.includes("commandExecution")
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
      ...(grantRoot ? { targetPath: grantRoot } : {}),
      ...(permissions ? { permissions } : {}),
    });
    const id =
      typeof params.approvalId === "string" ? params.approvalId : randomUUID();
    const summary = sanitizeSummary(
      reason ??
        command ??
        grantRoot ??
        `${type.replace("_", " ")} approval requested`,
      160,
      cwd,
    );
    const approval: ApprovalRequest = {
      id,
      taskId: task.id,
      type,
      title:
        type === "file_change"
          ? "File change approval"
          : type === "permissions"
            ? "Permissions approval"
            : type === "network"
              ? "Network approval"
              : "Command approval",
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

  desktopList(): DesktopApproval[] {
    return [...this.pending.values()].map(({ approval, request }) => ({
      approval,
      method: request.method,
      desktopDecisions: request.method.includes("permissions")
        ? ["decline", "cancel"]
        : ["accept", "acceptForSession", "decline", "cancel"],
    }));
  }

  respond(approvalId: string, decision: ApprovalDecision): ApprovalRequest {
    const pending = this.pending.get(approvalId);
    if (!pending) throw new Error(`Approval not found: ${approvalId}`);
    if (!pending.approval.allowedDecisions.includes(decision))
      throw new Error(`Decision not allowed: ${decision}`);
    pending.request.respond({ decision });
    return this.finish(approvalId, decision);
  }

  respondDesktop(approvalId: string, response: unknown): ApprovalRequest {
    const pending = this.pending.get(approvalId);
    if (!pending) throw new Error(`Approval not found: ${approvalId}`);
    const parsed = desktopResponseSchema.parse(response);
    pending.request.respond(parsed);
    const decision =
      "decision" in parsed && typeof parsed.decision === "string"
        ? parsed.decision
        : "accept";
    return this.finish(approvalId, decision);
  }

  private finish(approvalId: string, decision: string): ApprovalRequest {
    const pending = this.pending.get(approvalId);
    if (!pending) throw new Error(`Approval not found: ${approvalId}`);
    this.pending.delete(approvalId);
    const task = this.tasks.require(pending.approval.taskId);
    if (task.status === "waiting_approval") {
      this.tasks.transition(
        task.id,
        "running",
        decision === "accept" || decision === "acceptForSession"
          ? "Approval accepted"
          : "Approval declined",
      );
    }
    this.emit("resolved", { approvalId, decision });
    return pending.approval;
  }
}
