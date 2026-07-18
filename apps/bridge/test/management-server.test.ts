import { mkdtemp } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

import { afterEach, describe, expect, it, vi } from "vitest";

import { ApprovalService } from "../src/approvals/approval-service.js";
import { DesktopInputService } from "../src/desktop-input-service.js";
import { StructuredLogger } from "../src/logging/structured-logger.js";
import { ManagementServer } from "../src/management/management-server.js";
import { MessageRouter } from "../src/protocol/message-router.js";
import { ControlDeckServer } from "../src/server.js";
import { TaskRegistry } from "../src/tasks/task-registry.js";
import { WorkflowRegistry } from "../src/workflows/workflow-registry.js";

const managementServers: ManagementServer[] = [];
const deckServers: ControlDeckServer[] = [];

afterEach(async () => {
  await Promise.all(managementServers.splice(0).map((server) => server.stop()));
  await Promise.all(deckServers.splice(0).map((server) => server.stop()));
});

describe("companion management server", () => {
  it("binds to loopback and requires the per-launch bearer token", async () => {
    const tasks = new TaskRegistry();
    const approvals = new ApprovalService(tasks);
    const workflows = new WorkflowRegistry();
    const router = new MessageRouter(
      tasks,
      workflows,
      {
        stop: vi.fn(),
        followup: vi.fn(),
        launchWorkflow: vi.fn(),
        launchSkill: vi.fn(),
      },
      approvals,
    );
    const deck = new ControlDeckServer(
      "0.1.0",
      tasks,
      approvals,
      router,
      () => true,
    );
    deckServers.push(deck);
    await deck.start("127.0.0.1", 0, false);
    const directory = await mkdtemp(path.join(os.tmpdir(), "codexdeck-logs-"));
    const logger = new StructuredLogger(directory);
    const reload = vi.fn(async () => undefined);
    const management = new ManagementServer({
      token: "0123456789abcdef0123456789abcdef",
      bridgeVersion: "0.1.0",
      testedCodexVersion: "0.140.0",
      deckServer: deck,
      tasks,
      approvals,
      desktopInput: new DesktopInputService(tasks),
      logger,
      reload,
      shutdown: vi.fn(),
    });
    managementServers.push(management);
    const port = await management.start();

    const unauthorized = await fetch(`http://127.0.0.1:${port}/v1/status`);
    expect(unauthorized.status).toBe(401);
    const response = await fetch(`http://127.0.0.1:${port}/v1/status`, {
      headers: { Authorization: "Bearer 0123456789abcdef0123456789abcdef" },
    });
    expect(response.status).toBe(200);
    await expect(response.json()).resolves.toMatchObject({
      version: 1,
      lifecycle: "online",
      connectedDevices: 0,
    });

    const reloaded = await fetch(`http://127.0.0.1:${port}/v1/reload`, {
      method: "POST",
      headers: { Authorization: "Bearer 0123456789abcdef0123456789abcdef" },
    });
    expect(reloaded.status).toBe(200);
    expect(reload).toHaveBeenCalledOnce();

    const now = new Date().toISOString();
    tasks.create({
      id: "finished",
      threadId: "thread-finished",
      projectId: "demo",
      title: "Finished",
      status: "failed",
      summary: "Failed",
      startedAt: now,
      updatedAt: now,
    });
    const clearUnauthorized = await fetch(
      `http://127.0.0.1:${port}/v1/tasks/clear-finished`,
      { method: "POST" },
    );
    expect(clearUnauthorized.status).toBe(401);
    const cleared = await fetch(
      `http://127.0.0.1:${port}/v1/tasks/clear-finished`,
      {
        method: "POST",
        headers: {
          Authorization: "Bearer 0123456789abcdef0123456789abcdef",
        },
      },
    );
    expect(cleared.status).toBe(200);
    await expect(cleared.json()).resolves.toEqual({ cleared: 1 });
    expect(tasks.all()).toHaveLength(0);
  });
});
