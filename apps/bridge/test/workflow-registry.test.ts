import { mkdtemp, mkdir, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

import { describe, expect, it } from "vitest";

import { WorkflowRegistry } from "../src/workflows/workflow-registry.js";

describe("WorkflowRegistry", () => {
  it("loads strict allowlisted projects", async () => {
    const root = await mkdtemp(path.join(os.tmpdir(), "codexdeck-workflow-"));
    const project = path.join(root, "project");
    await mkdir(project);
    const file = path.join(root, "workflows.yaml");
    await writeFile(
      file,
      `projects:\n  demo:\n    label: Demo\n    cwd: ${project}\n    workflows:\n      review:\n        label: Review\n        shortLabel: Review\n        prompt: Review this repo.\n`,
    );
    const registry = new WorkflowRegistry();
    await registry.load(file);
    expect(registry.getWorkflow("demo", "review").prompt).toBe("Review this repo.");
  });

  it("rejects relative project paths and unknown fields", async () => {
    const root = await mkdtemp(path.join(os.tmpdir(), "codexdeck-workflow-"));
    const file = path.join(root, "workflows.yaml");
    await writeFile(
      file,
      "projects:\n  demo:\n    label: Demo\n    cwd: ./relative\n    surprise: true\n    workflows: {}\n",
    );
    const registry = new WorkflowRegistry();
    await expect(registry.load(file)).rejects.toThrow();
  });
});
