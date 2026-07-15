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
});
