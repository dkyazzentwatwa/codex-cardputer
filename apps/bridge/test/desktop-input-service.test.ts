import { describe, expect, it, vi } from "vitest";

import { DesktopInputService } from "../src/desktop-input-service.js";
import { TaskRegistry } from "../src/tasks/task-registry.js";

function managedTasks(): TaskRegistry {
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
  return tasks;
}

describe("DesktopInputService", () => {
  it("holds structured input for the companion and correlates its response", () => {
    const tasks = managedTasks();
    const respond = vi.fn();
    const service = new DesktopInputService(tasks);
    service.handle({
      id: 72,
      method: "item/tool/requestUserInput",
      params: {
        threadId: "thread-1",
        questions: [{ id: "name", question: "Project name?" }],
      },
      respond,
      reject: vi.fn(),
    });

    expect(service.list()).toHaveLength(1);
    expect(tasks.require("task-1").status).toBe("waiting_input");
    service.respond("72", { name: { answers: ["CodexDeck"] } });
    expect(respond).toHaveBeenCalledWith({
      answers: { name: { answers: ["CodexDeck"] } },
    });
    expect(service.list()).toEqual([]);
    expect(tasks.require("task-1").status).toBe("running");
  });

  it("rejects malformed desktop answers", () => {
    const tasks = managedTasks();
    const service = new DesktopInputService(tasks);
    service.handle({
      id: "input-1",
      method: "item/tool/requestUserInput",
      params: { threadId: "thread-1", questions: [] },
      respond: vi.fn(),
      reject: vi.fn(),
    });
    expect(() =>
      service.respond("input-1", { question: "raw string" }),
    ).toThrow();
  });
});
