import type { AppServerNotification } from "./types.js";
import { commandCategory } from "../tasks/summary-builder.js";
import type { TaskRegistry } from "../tasks/task-registry.js";

function record(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === "object"
    ? (value as Record<string, unknown>)
    : {};
}

function text(value: unknown): string | undefined {
  return typeof value === "string" && value.trim() ? value : undefined;
}

export class EventNormalizer {
  private readonly summaryPriority = new Map<string, number>();

  constructor(private readonly tasks: TaskRegistry) {}

  handle(notification: AppServerNotification): void {
    const params = record(notification.params);
    const threadId = text(params.threadId);
    if (!threadId) return;
    const task = this.tasks.byThread(threadId);
    if (!task) return;
    // External threads are deliberately monitor-only and are refreshed by
    // thread/list polling. Do not turn their events into mutable deck tasks.
    if (task.external) return;

    if (notification.method === "turn/started") {
      const turnId = text(record(params.turn).id);
      if (turnId) {
        this.summaryPriority.delete(task.id);
        this.tasks.beginTurn(task.id, turnId);
      }
      return;
    }

    if (notification.method === "turn/completed") {
      const status = text(record(params.turn).status) ?? "completed";
      if (status === "failed")
        this.tasks.transition(task.id, "failed", "Codex turn failed");
      else if (status === "interrupted" || status === "cancelled")
        this.tasks.transition(task.id, "cancelled", "Task cancelled");
      else this.tasks.transition(task.id, "completed", task.summary);
      return;
    }

    if (notification.method === "turn/plan/updated") {
      const plan = Array.isArray(params.plan) ? params.plan : [];
      const active = plan
        .map(record)
        .find((step) => step.status === "in_progress");
      const step = text(active?.step) ?? text(active?.text);
      if (step) this.update(task.id, step, 4);
      return;
    }

    if (
      notification.method !== "item/started" &&
      notification.method !== "item/completed"
    )
      return;
    const item = record(params.item);
    const itemType = text(item.type);
    if (
      itemType === "agentMessage" &&
      notification.method === "item/completed"
    ) {
      const message = text(item.text) ?? text(item.message);
      if (message) this.update(task.id, message, 5, true);
    } else if (itemType === "commandExecution") {
      const command = text(item.command) ?? "command";
      this.update(task.id, commandCategory(command), 2);
    } else if (itemType === "fileChange") {
      this.update(
        task.id,
        notification.method === "item/started"
          ? "Applying file changes"
          : "File changes applied",
        1,
      );
    }
  }

  private update(
    taskId: string,
    summary: string,
    priority: number,
    includeDetail = false,
  ): void {
    const current = this.summaryPriority.get(taskId) ?? 0;
    if (priority < current) return;
    this.summaryPriority.set(taskId, priority);
    this.tasks.updateSummary(
      taskId,
      summary,
      includeDetail ? summary : undefined,
    );
  }
}
