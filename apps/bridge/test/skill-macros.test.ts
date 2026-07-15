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

  it("round-robins skills so each configured project remains represented", async () => {
    const root = await mkdtemp(path.join(os.tmpdir(), "codexdeck-skills-"));
    const first = path.join(root, "first");
    const second = path.join(root, "second");
    await mkdir(first);
    await mkdir(second);
    const workflowFile = path.join(root, "workflows.yaml");
    await writeFile(
      workflowFile,
      `projects:\n  first:\n    label: First\n    cwd: ${first}\n    workflows: {}\n  second:\n    label: Second\n    cwd: ${second}\n    workflows: {}\n`,
    );
    const workflows = new WorkflowRegistry();
    await workflows.load(workflowFile);
    const listSkills = vi.fn(async (cwd: string) =>
      Array.from({ length: 20 }, (_, index) => ({
        name: `${path.basename(cwd)}-${String(index).padStart(2, "0")}`,
        path: `/skills/${index}`,
        enabled: true,
      })),
    );

    const macros = await discoverSkillMacros({ listSkills }, workflows);

    expect(macros).toHaveLength(20);
    expect(macros.slice(0, 4).map((macro) => macro.id)).toEqual([
      "skill:first/first-00",
      "skill:second/second-00",
      "skill:first/first-01",
      "skill:second/second-01",
    ]);
  });
});
