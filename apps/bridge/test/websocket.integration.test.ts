import { mkdtemp, mkdir, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

import WebSocket from "ws";
import { afterEach, describe, expect, it, vi } from "vitest";

import { ApprovalService } from "../src/approvals/approval-service.js";
import { MessageRouter } from "../src/protocol/message-router.js";
import { ControlDeckServer } from "../src/server.js";
import { TaskRegistry } from "../src/tasks/task-registry.js";
import { WorkflowRegistry } from "../src/workflows/workflow-registry.js";

const servers: ControlDeckServer[] = [];

afterEach(async () => {
  await Promise.all(servers.splice(0).map((server) => server.stop()));
});

async function waitFor(
  messages: Array<Record<string, unknown>>,
  predicate: () => boolean,
): Promise<void> {
  const started = Date.now();
  while (!predicate()) {
    if (Date.now() - started > 2000)
      throw new Error(
        `Timed out waiting for messages: ${JSON.stringify(messages)}`,
      );
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
}

describe("device WebSocket", () => {
  it("handshakes, snapshots, stays alive after invalid input, and deduplicates mutations", async () => {
    const root = await mkdtemp(path.join(os.tmpdir(), "codexdeck-ws-"));
    const project = path.join(root, "project");
    await mkdir(project);
    const workflowFile = path.join(root, "workflows.yaml");
    await writeFile(
      workflowFile,
      `projects:\n  demo:\n    label: Demo\n    cwd: ${project}\n    workflows:\n      review:\n        label: Review\n        shortLabel: Review\n        prompt: Review this project.\n`,
    );
    const workflows = new WorkflowRegistry();
    await workflows.load(workflowFile);
    const tasks = new TaskRegistry();
    const approvals = new ApprovalService(tasks);
    const launchWorkflow = vi.fn(async () => ({}));
    const router = new MessageRouter(
      tasks,
      workflows,
      {
        stop: vi.fn(async () => ({})),
        followup: vi.fn(async () => ({})),
        launchWorkflow,
        launchSkill: vi.fn(async () => ({})),
      },
      approvals,
    );
    const server = new ControlDeckServer(
      "0.1.0",
      tasks,
      approvals,
      router,
      () => true,
    );
    servers.push(server);
    await server.start("127.0.0.1", 0, false);
    const address = server.address();
    expect(address).toBeTruthy();
    const port = Number(address?.split(":").at(-1));
    const socket = new WebSocket(`ws://127.0.0.1:${port}/device`);
    const messages: Array<Record<string, unknown>> = [];
    socket.on("message", (data) =>
      messages.push(JSON.parse(data.toString()) as Record<string, unknown>),
    );
    await new Promise<void>((resolve, reject) => {
      socket.once("open", () => resolve());
      socket.once("error", reject);
    });
    socket.send(
      JSON.stringify({
        type: "hello",
        protocol: "codexdeck.v1",
        deviceId: "cardputer-test",
        deviceName: "CardPuter",
        firmwareVersion: "0.1.0",
        capabilities: ["keyboard", "display", "hold-confirm"],
      }),
    );
    await waitFor(messages, () =>
      messages.some((message) => message.type === "macro.snapshot"),
    );
    expect(messages.map((message) => message.type)).toEqual(
      expect.arrayContaining(["welcome", "task.snapshot", "macro.snapshot"]),
    );

    const second = new WebSocket(`ws://127.0.0.1:${port}/device`);
    const secondMessages: Array<Record<string, unknown>> = [];
    second.on("message", (data) =>
      secondMessages.push(
        JSON.parse(data.toString()) as Record<string, unknown>,
      ),
    );
    await new Promise<void>((resolve, reject) => {
      second.once("open", () => resolve());
      second.once("error", reject);
    });
    second.send(
      JSON.stringify({
        type: "hello",
        protocol: "codexdeck.v1",
        deviceId: "cardputer-second",
        deviceName: "CardPuter 2",
        firmwareVersion: "0.1.0",
        capabilities: ["keyboard", "display", "hold-confirm"],
      }),
    );
    await waitFor(secondMessages, () =>
      secondMessages.some((message) => message.type === "task.snapshot"),
    );
    const now = new Date().toISOString();
    tasks.create({
      id: "task-live",
      threadId: "thread-live",
      turnId: "turn-live",
      projectId: "demo",
      title: "Live task",
      status: "running",
      summary: "Working",
      startedAt: now,
      updatedAt: now,
    });
    const receivedLiveTask = (items: Array<Record<string, unknown>>) =>
      items.some(
        (message) =>
          message.type === "task.upsert" &&
          (message.task as { id?: string } | undefined)?.id === "task-live",
      );
    await waitFor(messages, () => receivedLiveTask(messages));
    await waitFor(secondMessages, () => receivedLiveTask(secondMessages));
    await new Promise<void>((resolve) => {
      second.once("close", () => resolve());
      second.close();
    });

    const reconnected = new WebSocket(`ws://127.0.0.1:${port}/device`);
    const reconnectMessages: Array<Record<string, unknown>> = [];
    reconnected.on("message", (data) =>
      reconnectMessages.push(
        JSON.parse(data.toString()) as Record<string, unknown>,
      ),
    );
    await new Promise<void>((resolve, reject) => {
      reconnected.once("open", () => resolve());
      reconnected.once("error", reject);
    });
    reconnected.send(
      JSON.stringify({
        type: "hello",
        protocol: "codexdeck.v1",
        deviceId: "cardputer-second",
        deviceName: "CardPuter 2",
        firmwareVersion: "0.1.0",
        capabilities: ["keyboard", "display", "hold-confirm"],
      }),
    );
    await waitFor(reconnectMessages, () =>
      reconnectMessages.some((message) => {
        if (message.type !== "task.snapshot") return false;
        const snapshot = message.tasks as Array<{ id?: string }> | undefined;
        return snapshot?.some((task) => task.id === "task-live") ?? false;
      }),
    );

    socket.send("{");
    await waitFor(messages, () =>
      messages.some((message) => message.type === "error"),
    );
    expect(socket.readyState).toBe(WebSocket.OPEN);

    const request = {
      type: "workflow.launch.request",
      requestId: "req-1",
      projectId: "demo",
      workflowId: "review",
    };
    socket.send(JSON.stringify(request));
    await waitFor(
      messages,
      () =>
        messages.filter((message) => message.requestId === "req-1").length ===
        1,
    );
    socket.send(JSON.stringify(request));
    await waitFor(
      messages,
      () =>
        messages.filter((message) => message.requestId === "req-1").length ===
        2,
    );
    expect(launchWorkflow).toHaveBeenCalledTimes(1);

    const health = await fetch(`http://127.0.0.1:${port}/healthz`).then(
      (response) => response.json(),
    );
    expect(health).toMatchObject({ status: "ok", connectedDevices: 2 });
    reconnected.close();
    socket.close();
  });
});
