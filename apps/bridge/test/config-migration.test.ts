import { mkdir, mkdtemp, readFile, stat, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

import { describe, expect, it } from "vitest";

import { migrateConfiguration } from "../src/management/migrate-config.js";

describe("companion configuration migration", () => {
  it("imports legacy YAML into secure versioned JSON without changing the source", async () => {
    const root = await mkdtemp(path.join(os.tmpdir(), "codexdeck-migrate-"));
    const project = path.join(root, "project");
    await mkdir(project);
    const bridge = path.join(root, "bridge.yaml");
    const workflows = path.join(root, "workflows.yaml");
    const destination = path.join(root, "managed");
    const bridgeSource =
      "port: 9000\ncodexPath: /usr/local/bin/codex\nworkflowsFile: workflows.yaml\n";
    const workflowSource = `projects:\n  demo:\n    label: Demo\n    cwd: ${project}\n    workflows:\n      review:\n        label: Review\n        shortLabel: Review\n        prompt: Review the project.\n`;
    await writeFile(bridge, bridgeSource);
    await writeFile(workflows, workflowSource);

    await migrateConfiguration(bridge, workflows, destination);

    const settings = JSON.parse(
      await readFile(path.join(destination, "settings.json"), "utf8"),
    ) as Record<string, unknown>;
    const registry = JSON.parse(
      await readFile(path.join(destination, "workflows.json"), "utf8"),
    ) as { version: number; projects: Record<string, unknown> };
    expect(settings).toMatchObject({
      version: 1,
      port: 9000,
      codexPath: "/usr/local/bin/codex",
      workflowsFile: "workflows.json",
    });
    expect(registry.version).toBe(1);
    expect(registry.projects.demo).toBeTruthy();
    expect(
      (await stat(path.join(destination, "settings.json"))).mode & 0o777,
    ).toBe(0o600);
    await expect(readFile(bridge, "utf8")).resolves.toBe(bridgeSource);
    await expect(readFile(workflows, "utf8")).resolves.toBe(workflowSource);
  });
});
