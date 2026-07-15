import { mkdtemp, mkdir, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

import { describe, expect, it, vi } from "vitest";

import { discoverSkillMacros } from "../src/workflows/skill-macros.js";
import { WorkflowRegistry } from "../src/workflows/workflow-registry.js";

describe("skill macro discovery", () => {
  it("exposes enabled App Server skills without leaking their paths", async () => {
    const root = await mkdtemp(path.join(os.tmpdir(), "codexdeck-skills-"));
    const project = path.join(root, "project");
    await mkdir(project);
    const workflowFile = path.join(root, "workflows.yaml");
    await writeFile(
      workflowFile,
      `projects:\n  demo:\n    label: Demo\n    cwd: ${project}\n    workflows: {}\n`,
    );
    const workflows = new WorkflowRegistry();
    await workflows.load(workflowFile);
    const listSkills = vi.fn(async () => [
      { name: "review", path: "/secret/skills/review/SKILL.md", enabled: true },
    ]);

    const macros = await discoverSkillMacros({ listSkills }, workflows);

    expect(macros).toEqual([
      {
        id: "skill:demo/review",
        label: "Demo: review",
        shortLabel: "review",
        action: "launch_skill",
        confirmation: "none",
        enabled: true,
      },
    ]);
    expect(JSON.stringify(macros)).not.toContain("/secret/");
  });
});
