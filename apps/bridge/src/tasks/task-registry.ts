import { EventEmitter } from "node:events";

import type {
  MacroDescriptor,
  TaskStatus,
  TaskSummary,
} from "@codexdeck/protocol";

import { sanitizeSummary } from "./summary-builder.js";

export interface ManagedTask extends TaskSummary {
  workflowId?: string;
  previousActiveStatus?: TaskStatus;
  external?: boolean;
  externalSource?: string;
}

const CLEARABLE_STATUSES = new Set<TaskStatus>([
  "completed",
  "failed",
  "cancelled",
]);

export function toTaskSummary(task: ManagedTask): TaskSummary {
  const summary: ManagedTask = { ...task };
  delete summary.workflowId;
  delete summary.previousActiveStatus;
  delete summary.external;
  delete summary.externalSource;
  return summary;
}

const VALID_TRANSITIONS: Record<TaskStatus, Set<TaskStatus>> = {
  starting: new Set(["running", "failed", "cancelled", "stale"]),
  running: new Set([
    "waiting_approval",
    "waiting_input",
    "completed",
    "failed",
    "cancelled",
    "stale",
  ]),
  waiting_approval: new Set(["running", "failed", "cancelled", "stale"]),
  waiting_input: new Set(["running", "failed", "cancelled", "stale"]),
  completed: new Set(),
  failed: new Set(),
  cancelled: new Set(),
  stale: new Set([
    "starting",
    "running",
    "waiting_approval",
    "waiting_input",
    "failed",
    "cancelled",
  ]),
};

export function macrosForStatus(status: TaskStatus): MacroDescriptor[] {
  const macro = (
    id: string,
    label: string,
    shortLabel: string,
    action: MacroDescriptor["action"],
    confirmation: MacroDescriptor["confirmation"] = "none",
  ): MacroDescriptor => ({
    id,
    label,
    shortLabel,
    action,
    confirmation,
    enabled: true,
  });
  switch (status) {
    case "starting":
    case "running":
      return [
        macro("stop", "Stop task", "Stop", "stop_task", "press_again"),
        macro("followup", "Send follow-up", "Follow-up", "send_followup"),
      ];
    case "waiting_approval":
      return [
        macro(
          "approve",
          "Approve request",
          "Approve",
          "approve",
          "press_again",
        ),
        macro("reject", "Reject request", "Reject", "reject"),
      ];
    case "waiting_input":
      return [];
    case "failed":
      return [
        macro("retry", "Retry workflow", "Retry", "retry", "press_again"),
      ];
    case "completed":
      return [
        macro("review", "Review changes", "Review", "review_changes"),
        macro("tests", "Run tests", "Tests", "run_tests", "press_again"),
        macro("followup", "Send follow-up", "Follow-up", "send_followup"),
      ];
    case "cancelled":
    case "stale":
      return [];
  }
  throw new Error(`Unsupported task status: ${String(status)}`);
}

export class TaskRegistry extends EventEmitter {
  private readonly tasks = new Map<string, ManagedTask>();
  private readonly threadIndex = new Map<string, string>();

  create(
    task: Omit<ManagedTask, "elapsedSeconds" | "requiresAttention" | "macros">,
  ): ManagedTask {
    if (this.tasks.has(task.id))
      throw new Error(`Duplicate task id: ${task.id}`);
    const created: ManagedTask = {
      ...task,
      elapsedSeconds: 0,
      requiresAttention:
        task.status === "waiting_approval" || task.status === "waiting_input",
      macros: macrosForStatus(task.status),
    };
    this.tasks.set(created.id, created);
    this.threadIndex.set(created.threadId, created.id);
    this.emitTask(created);
    return created;
  }

  createExternal(
    task: Omit<ManagedTask, "elapsedSeconds" | "requiresAttention" | "macros" | "external">,
  ): ManagedTask {
    if (this.tasks.has(task.id)) return this.require(task.id);
    const created: ManagedTask = {
      ...task,
      external: true,
      elapsedSeconds: 0,
      requiresAttention:
        task.status === "waiting_approval" || task.status === "waiting_input",
      macros: [],
    };
    this.tasks.set(created.id, created);
    this.threadIndex.set(created.threadId, created.id);
    this.emitTask(created);
    return created;
  }

  updateExternal(taskId: string, status: TaskStatus, summary: string): ManagedTask {
    const task = this.require(taskId);
    if (!task.external) throw new Error("Task is not externally monitored");
    task.status = status;
    task.summary = sanitizeSummary(summary, 64);
    task.updatedAt = new Date().toISOString();
    task.requiresAttention = status === "waiting_approval" || status === "waiting_input";
    task.macros = [];
    this.emitTask(task);
    return task;
  }

  remove(taskId: string): boolean {
    const task = this.tasks.get(taskId);
    if (!task) return false;
    this.tasks.delete(taskId);
    if (this.threadIndex.get(task.threadId) === taskId) this.threadIndex.delete(task.threadId);
    this.emit("remove", taskId);
    return true;
  }

  restore(tasks: ManagedTask[]): void {
    for (const task of tasks) {
      this.tasks.set(task.id, task);
      this.threadIndex.set(task.threadId, task.id);
    }
  }

  get(taskId: string): ManagedTask | undefined {
    return this.tasks.get(taskId);
  }

  require(taskId: string): ManagedTask {
    const task = this.get(taskId);
    if (!task) throw new Error(`Task not found: ${taskId}`);
    return task;
  }

  byThread(threadId: string): ManagedTask | undefined {
    const taskId = this.threadIndex.get(threadId);
    return taskId ? this.tasks.get(taskId) : undefined;
  }

  all(): ManagedTask[] {
    const now = Date.now();
    return [...this.tasks.values()]
      .map((task) => ({
        ...task,
        elapsedSeconds: Math.max(
          0,
          Math.floor((now - Date.parse(task.startedAt)) / 1000),
        ),
      }))
      .sort((left, right) => {
        if (left.requiresAttention !== right.requiresAttention)
          return left.requiresAttention ? -1 : 1;
        if ((left.status === "running") !== (right.status === "running"))
          return left.status === "running" ? -1 : 1;
        return Date.parse(right.updatedAt) - Date.parse(left.updatedAt);
      });
  }

  clearableCount(): number {
    let count = 0;
    for (const task of this.tasks.values()) {
      if (CLEARABLE_STATUSES.has(task.status)) ++count;
    }
    return count;
  }

  clearFinished(): string[] {
    const removed: string[] = [];
    for (const [taskId, task] of this.tasks) {
      if (!CLEARABLE_STATUSES.has(task.status)) continue;
      this.tasks.delete(taskId);
      if (this.threadIndex.get(task.threadId) === taskId)
        this.threadIndex.delete(task.threadId);
      removed.push(taskId);
      this.emit("remove", taskId);
    }
    return removed;
  }

  transition(
    taskId: string,
    status: TaskStatus,
    summary?: string,
  ): ManagedTask {
    const task = this.require(taskId);
    const allowed = VALID_TRANSITIONS[task.status];
    if (task.status !== status && !allowed.has(status)) {
      throw new Error(`Invalid task transition: ${task.status} -> ${status}`);
    }
    if (status === "stale" && task.status !== "stale")
      task.previousActiveStatus = task.status;
    task.status = status;
    if (summary) task.summary = sanitizeSummary(summary);
    task.updatedAt = new Date().toISOString();
    task.requiresAttention =
      status === "waiting_approval" || status === "waiting_input";
    task.macros = macrosForStatus(status);
    if (status !== "waiting_approval") delete task.pendingApprovalId;
    this.emitTask(task);
    return task;
  }

  beginTurn(taskId: string, turnId: string): ManagedTask {
    const task = this.require(taskId);
    task.turnId = turnId;
    task.status = "running";
    task.summary = "Working on task";
    task.updatedAt = new Date().toISOString();
    task.requiresAttention = false;
    task.macros = macrosForStatus("running");
    delete task.pendingApprovalId;
    delete task.detail;
    this.emitTask(task);
    return task;
  }

  updateSummary(taskId: string, summary: string, detail?: string): ManagedTask {
    const task = this.require(taskId);
    task.summary = sanitizeSummary(summary, 64);
    if (detail) task.detail = sanitizeSummary(detail, 768);
    task.updatedAt = new Date().toISOString();
    this.emitTask(task);
    return task;
  }

  attachApproval(
    taskId: string,
    approvalId: string,
    summary: string,
  ): ManagedTask {
    const task = this.transition(taskId, "waiting_approval", summary);
    task.pendingApprovalId = approvalId;
    this.emitTask(task);
    return task;
  }

  markActiveStale(): void {
    for (const task of this.tasks.values()) {
      if (
        ["starting", "running", "waiting_approval", "waiting_input"].includes(
          task.status,
        )
      ) {
        this.transition(task.id, "stale", "Codex connection interrupted");
      }
    }
  }

  private emitTask(task: ManagedTask): void {
    this.emit("upsert", { ...task });
  }
}
