import { describe, expect, it, vi } from "vitest";

import type { AppServerClient } from "../src/codex/app-server-client.js";
import { ExternalSessionMonitor } from "../src/codex/external-session-monitor.js";
import { TaskRegistry } from "../src/tasks/task-registry.js";

function fakeClient(threads: unknown[]) {
  return {
    ready: true,
    listThreads: vi.fn().mockResolvedValue(threads),
  } as unknown as AppServerClient;
}

const monitorOptions = {
  enabled: true,
  limit: 8,
  cwd: [],
  sourceKinds: ["cli", "vscode", "exec", "appServer", "unknown"],
};

describe("ExternalSessionMonitor", () => {
  it("mirrors active external sessions as monitor-only tasks", async () => {
    const tasks = new TaskRegistry();
    const client = fakeClient([
      {
        id: "external-thread",
        name: "Investigate deployment",
        source: "appServer",
        createdAt: 1_700_000_000,
        updatedAt: 1_700_000_100,
        status: { type: "active", activeFlags: ["waitingOnApproval"] },
      },
    ]);
    const monitor = new ExternalSessionMonitor(client, tasks, monitorOptions);

    await monitor.refresh();

    const task = tasks.require("external-external-thread");
    expect(task).toMatchObject({
      external: true,
      threadId: "external-thread",
      status: "waiting_approval",
      requiresAttention: true,
      macros: [],
    });
    expect(task.summary).toContain("desktop");
  });

  it("removes sessions when they are no longer active", async () => {
    const tasks = new TaskRegistry();
    const client = fakeClient([
      {
        id: "external-thread",
        source: "cli",
        status: { type: "active" },
      },
    ]);
    const monitor = new ExternalSessionMonitor(client, tasks, monitorOptions);
    await monitor.refresh();
    vi.mocked(client.listThreads).mockResolvedValue([]);

    await monitor.refresh();

    expect(tasks.all()).toEqual([]);
  });

  it("does not duplicate a thread already managed by CodexDeck", async () => {
    const tasks = new TaskRegistry();
    const now = new Date().toISOString();
    tasks.create({
      id: "managed",
      threadId: "shared-thread",
      projectId: "project",
      title: "Bridge task",
      status: "running",
      summary: "Working",
      startedAt: now,
      updatedAt: now,
    });
    const client = fakeClient([
      {
        id: "shared-thread",
        source: "cli",
        status: { type: "active" },
      },
    ]);

    await new ExternalSessionMonitor(client, tasks, monitorOptions).refresh();

    expect(tasks.all().map((task) => task.id)).toEqual(["managed"]);
  });
});
