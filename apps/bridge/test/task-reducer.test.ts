import { describe, expect, it } from "vitest";

import { TaskRegistry } from "../src/tasks/task-registry.js";

function createTask(
  registry: TaskRegistry,
  id: string,
  status: "starting" | "running" = "starting",
) {
  const now = new Date().toISOString();
  return registry.create({
    id,
    threadId: `thread-${id}`,
    projectId: "project",
    title: id,
    status,
    summary: "Starting",
    startedAt: now,
    updatedAt: now,
  });
}

describe("TaskRegistry", () => {
  it("enforces transitions and terminal stability", () => {
    const registry = new TaskRegistry();
    createTask(registry, "one");
    registry.transition("one", "running");
    registry.transition("one", "waiting_approval");
    registry.transition("one", "running");
    registry.transition("one", "completed");
    expect(() => registry.transition("one", "running")).toThrow(
      /Invalid task transition/,
    );
  });

  it("sorts attention tasks above running tasks", () => {
    const registry = new TaskRegistry();
    createTask(registry, "running", "running");
    createTask(registry, "attention", "running");
    registry.transition("attention", "waiting_input", "Answer on desktop");
    expect(registry.all().map((task) => task.id)).toEqual([
      "attention",
      "running",
    ]);
  });

  it("marks active tasks stale and restores the prior state", () => {
    const registry = new TaskRegistry();
    createTask(registry, "one", "running");
    registry.markActiveStale();
    expect(registry.require("one").previousActiveStatus).toBe("running");
    registry.transition("one", "running", "Resumed");
    expect(registry.require("one").status).toBe("running");
  });

  it("clears only terminal history and emits removals", () => {
    const registry = new TaskRegistry();
    const removed: string[] = [];
    registry.on("remove", (taskId: string) => removed.push(taskId));
    createTask(registry, "active", "running");
    createTask(registry, "stale", "running");
    registry.markActiveStale();
    registry.transition("active", "running", "Resumed");
    createTask(registry, "completed", "running");
    registry.transition("completed", "completed");
    createTask(registry, "failed");
    registry.transition("failed", "failed");
    createTask(registry, "cancelled");
    registry.transition("cancelled", "cancelled");

    expect(registry.clearableCount()).toBe(3);
    expect(registry.clearFinished()).toEqual([
      "completed",
      "failed",
      "cancelled",
    ]);
    expect(removed).toEqual(["completed", "failed", "cancelled"]);
    expect(
      registry
        .all()
        .map((task) => task.id)
        .sort(),
    ).toEqual(["active", "stale"]);
    expect(registry.byThread("thread-completed")).toBeUndefined();
    expect(registry.clearFinished()).toEqual([]);
  });
});
