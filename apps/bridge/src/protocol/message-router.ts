import type {
  DeviceMessage,
  MacroDescriptor,
  ServerMessage,
} from "@codexdeck/protocol";

import type { ApprovalDecision } from "@codexdeck/protocol";
import { sanitizeSummary } from "../tasks/summary-builder.js";
import { toTaskSummary, type TaskRegistry } from "../tasks/task-registry.js";
import type { WorkflowRegistry } from "../workflows/workflow-registry.js";
import { RequestCache } from "./request-cache.js";

export interface RouterRunner {
  stop(taskId: string): Promise<unknown>;
  followup(taskId: string, prompt: string): Promise<unknown>;
  launchWorkflow(projectId: string, workflowId: string): Promise<unknown>;
  launchSkill(
    projectId: string,
    skillName: string,
    prompt?: string,
  ): Promise<unknown>;
}

export interface RouterApprovals {
  respond(approvalId: string, decision: ApprovalDecision): unknown;
}

function errorMessage(error: unknown, requestId?: string): ServerMessage {
  return {
    type: "error",
    ...(requestId ? { requestId } : {}),
    code: "REQUEST_FAILED",
    message:
      error instanceof Error
        ? sanitizeSummary(error.message, 160)
        : "Request failed",
    recoverable: true,
  };
}

export class MessageRouter {
  private readonly caches = new Map<string, RequestCache>();
  private skillMacros: MacroDescriptor[] = [];

  constructor(
    private readonly tasks: TaskRegistry,
    private readonly workflows: WorkflowRegistry,
    private readonly runner: RouterRunner,
    private readonly approvals: RouterApprovals,
  ) {}

  macroSnapshot(): MacroDescriptor[] {
    const workflowMacros = this.workflows.list().flatMap((project) =>
      Object.entries(project.workflows).map(([workflowId, workflow]) => ({
        id: `${project.id}/${workflowId}`,
        label: `${project.label}: ${workflow.label}`.slice(0, 48),
        shortLabel: workflow.shortLabel,
        action: "launch_workflow" as const,
        confirmation: "none" as const,
        enabled: true,
      })),
    );
    return [...workflowMacros, ...this.skillMacros].slice(0, 20);
  }

  setSkillMacros(macros: MacroDescriptor[]): void {
    this.skillMacros = macros
      .filter((macro) => macro.action === "launch_skill")
      .slice(0, 20);
  }

  async handle(
    deviceId: string,
    message: DeviceMessage,
  ): Promise<ServerMessage[]> {
    if (!("requestId" in message)) return this.handleRead(message);
    const cache = this.cacheFor(deviceId);
    const cached = cache.get(message.requestId);
    if (cached) return cached;

    let responses: ServerMessage[];
    try {
      if (
        (message.type === "task.stop.request" || message.type === "task.followup.submit") &&
        this.tasks.get(message.taskId)?.external
      ) {
        throw new Error("External sessions are monitor-only. Adopt it on desktop before controlling it.");
      }
      switch (message.type) {
        case "task.stop.request":
          await this.runner.stop(message.taskId);
          responses = [
            {
              type: "toast",
              level: "success",
              message: "Task stopped",
              requestId: message.requestId,
            },
          ];
          break;
        case "task.followup.submit":
          await this.runner.followup(message.taskId, message.prompt);
          responses = [
            {
              type: "toast",
              level: "success",
              message: "Follow-up sent",
              requestId: message.requestId,
            },
          ];
          break;
        case "workflow.launch.request":
          await this.runner.launchWorkflow(
            message.projectId,
            message.workflowId,
          );
          responses = [
            {
              type: "toast",
              level: "success",
              message: "Workflow launched",
              requestId: message.requestId,
            },
          ];
          break;
        case "skill.launch.request":
          await this.runner.launchSkill(
            message.projectId,
            message.skillName,
            message.prompt,
          );
          responses = [
            {
              type: "toast",
              level: "success",
              message: "Skill launched",
              requestId: message.requestId,
            },
          ];
          break;
        case "approval.respond":
          this.approvals.respond(message.approvalId, message.decision);
          responses = [
            {
              type: "toast",
              level: "success",
              message: "Approval response sent",
              requestId: message.requestId,
            },
          ];
          break;
        case "tasks.clear.request": {
          const cleared = this.tasks.clearFinished().length;
          responses = [
            {
              type: "toast",
              level: cleared ? "success" : "info",
              message: cleared
                ? `Cleared ${cleared} finished task${cleared === 1 ? "" : "s"}`
                : "No finished tasks to clear",
              requestId: message.requestId,
            },
          ];
          break;
        }
      }
    } catch (error) {
      responses = [errorMessage(error, message.requestId)];
    }
    cache.set(message.requestId, responses);
    return responses;
  }

  private handleRead(message: DeviceMessage): ServerMessage[] {
    if (message.type === "snapshot.request") {
      return [
        {
          type: "task.snapshot",
          tasks: this.tasks.all().slice(0, 20).map(toTaskSummary),
        },
        { type: "macro.snapshot", macros: this.macroSnapshot() },
      ];
    }
    return [];
  }

  private cacheFor(deviceId: string): RequestCache {
    let cache = this.caches.get(deviceId);
    if (!cache) {
      cache = new RequestCache();
      this.caches.set(deviceId, cache);
    }
    return cache;
  }
}
