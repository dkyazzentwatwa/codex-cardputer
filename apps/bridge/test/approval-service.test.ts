import { describe, expect, it, vi } from "vitest";

import { ApprovalService } from "../src/approvals/approval-service.js";
import { classifyRisk } from "../src/approvals/risk-classifier.js";
import { TaskRegistry } from "../src/tasks/task-registry.js";

describe("approval safety", () => {
  it("classifies destructive operations as high risk", () => {
    expect(classifyRisk({ command: "sudo rm -rf /" })).toEqual({
      risk: "high",
      reason: "Recursive deletion",
    });
    expect(classifyRisk({ command: "pnpm test" }).risk).toBe("low");
  });

  it("relays only a permitted decision", () => {
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
    const respond = vi.fn();
    const service = new ApprovalService(tasks);
    const approval = service.open({
      id: 1,
      method: "item/commandExecution/requestApproval",
      params: { threadId: "thread-1", command: "pnpm test", cwd: "/tmp/project" },
      respond,
      reject: vi.fn(),
    });
    expect(approval?.allowedDecisions).toEqual(["accept", "decline", "cancel"]);
    service.respond(approval?.id ?? "", "accept");
    expect(respond).toHaveBeenCalledWith({ decision: "accept" });
    expect(tasks.require("task-1").status).toBe("running");
  });
});
