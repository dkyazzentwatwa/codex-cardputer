import { mkdtemp, mkdir, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

import { describe, expect, it, vi } from "vitest";

import { TaskRegistry } from "../src/tasks/task-registry.js";
import { WorkflowRegistry } from "../src/workflows/workflow-registry.js";
import { WorkflowRunner } from "../src/workflows/workflow-runner.js";

describe("WorkflowRunner", () => {
  it("starts an allowlisted thread and uses a structured skill input", async () => {
    const root = await mkdtemp(path.join(os.tmpdir(), "codexdeck-runner-"));
    const project = path.join(root, "project");
    await mkdir(project);
    const workflowFile = path.join(root, "workflows.yaml");
    await writeFile(
      workflowFile,
      `projects:\n  demo:\n    label: Demo\n    cwd: ${project}\n    approvalPolicy: on-request\n    sandbox: workspace-write\n    workflows:\n      review:\n        label: Review\n        shortLabel: Review\n        skill: security-review\n        prompt: Review this project.\n`,
    );
    const workflows = new WorkflowRegistry();
    await workflows.load(workflowFile);
    const startThread = vi.fn(async () => ({ id: "thread-1" }));
    const startTurn = vi.fn(async () => ({ id: "turn-1" }));
    const tasks = new TaskRegistry();
    const runner = new WorkflowRunner(
      {
        startThread,
        startTurn,
        steerTurn: vi.fn(async () => undefined),
        interruptTurn: vi.fn(async () => undefined),
        listSkills: vi.fn(async () => [
          {
            name: "security-review",
            path: "/skills/security-review/SKILL.md",
            enabled: true,
          },
        ]),
      },
      workflows,
      tasks,
    );

    const task = await runner.launchWorkflow("demo", "review");

    expect(startThread).toHaveBeenCalledWith({
      cwd: project,
      approvalPolicy: "on-request",
      sandbox: "workspace-write",
    });
    expect(startTurn).toHaveBeenCalledWith("thread-1", [
      {
        type: "skill",
        name: "security-review",
        path: "/skills/security-review/SKILL.md",
      },
      { type: "text", text: "Review this project.", text_elements: [] },
    ]);
    expect(task).toMatchObject({
      threadId: "thread-1",
      turnId: "turn-1",
      status: "running",
      workflowId: "review",
    });
  });
});
