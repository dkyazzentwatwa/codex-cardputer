import type { AppServerNotification } from "./types.js";
import { commandCategory, sanitizeSummary } from "../tasks/summary-builder.js";
import type { TaskRegistry } from "../tasks/task-registry.js";

function record(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === "object" ? (value as Record<string, unknown>) : {};
}

function text(value: unknown): string | undefined {
  return typeof value === "string" && value.trim() ? value : undefined;
}

export class EventNormalizer {
  constructor(private readonly tasks: TaskRegistry) {}

  handle(notification: AppServerNotification): void {
    const params = record(notification.params);
    const threadId = text(params.threadId);
    if (!threadId) return;
    const task = this.tasks.byThread(threadId);
    if (!task) return;

    if (notification.method === "turn/started") {
      const turnId = text(record(params.turn).id);
      if (turnId) this.tasks.beginTurn(task.id, turnId);
      return;
    }

    if (notification.method === "turn/completed") {
      const status = text(record(params.turn).status) ?? "completed";
      if (status === "failed") this.tasks.transition(task.id, "failed", "Codex turn failed");
      else if (status === "interrupted" || status === "cancelled")
        this.tasks.transition(task.id, "cancelled", "Task cancelled");
      else this.tasks.transition(task.id, "completed", task.summary);
      return;
    }

    if (notification.method === "turn/plan/updated") {
      const plan = Array.isArray(params.plan) ? params.plan : [];
      const active = plan.map(record).find((step) => step.status === "in_progress");
      const step = text(active?.step) ?? text(active?.text);
      if (step) this.tasks.updateSummary(task.id, step);
      return;
    }

    if (notification.method !== "item/started" && notification.method !== "item/completed") return;
    const item = record(params.item);
    const itemType = text(item.type);
    if (itemType === "agentMessage" && notification.method === "item/completed") {
      const message = text(item.text) ?? text(item.message);
      if (message) this.tasks.updateSummary(task.id, sanitizeSummary(message));
    } else if (itemType === "commandExecution") {
      const command = text(item.command) ?? "command";
      this.tasks.updateSummary(task.id, commandCategory(command));
    } else if (itemType === "fileChange") {
      this.tasks.updateSummary(
        task.id,
        notification.method === "item/started" ? "Applying file changes" : "File changes applied",
      );
    }
  }
}
