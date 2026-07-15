import { describe, expect, it } from "vitest";

import { EventNormalizer } from "../src/codex/event-normalizer.js";
import { TaskRegistry } from "../src/tasks/task-registry.js";

describe("EventNormalizer", () => {
  it("maps events deterministically and preserves higher-priority plan progress", () => {
    const tasks = new TaskRegistry();
    const now = new Date().toISOString();
    tasks.create({
      id: "task-1",
      threadId: "thread-1",
      turnId: "turn-1",
      projectId: "project",
      title: "Task",
      status: "running",
      summary: "Working",
      startedAt: now,
      updatedAt: now,
    });
    const normalizer = new EventNormalizer(tasks);

    normalizer.handle({
      method: "item/started",
      params: {
        threadId: "thread-1",
        item: { type: "commandExecution", command: "pnpm test -- --run" },
      },
    });
    expect(tasks.require("task-1").summary).toBe("Running project checks");

    normalizer.handle({
      method: "turn/plan/updated",
      params: {
        threadId: "thread-1",
        plan: [
          { step: "Implement the protocol adapter", status: "in_progress" },
        ],
      },
    });
    normalizer.handle({
      method: "item/completed",
      params: {
        threadId: "thread-1",
        item: { type: "agentMessage", text: "A lower priority interim note" },
      },
    });
    expect(tasks.require("task-1").summary).toBe(
      "Implement the protocol adapter",
    );

    normalizer.handle({
      method: "turn/completed",
      params: { threadId: "thread-1", turn: { status: "completed" } },
    });
    expect(tasks.require("task-1").status).toBe("completed");
  });
});
