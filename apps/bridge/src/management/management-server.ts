import { timingSafeEqual } from "node:crypto";
import { EventEmitter } from "node:events";

import Fastify from "fastify";
import { WebSocketServer } from "ws";

import type { ApprovalService } from "../approvals/approval-service.js";
import type { DesktopInputService } from "../desktop-input-service.js";
import type { StructuredLogger } from "../logging/structured-logger.js";
import type { ControlDeckServer } from "../server.js";
import type { ManagedTask, TaskRegistry } from "../tasks/task-registry.js";

export type ManagedLifecycleState = "online" | "degraded";

export interface ManagedBridgeStatus {
  version: 1;
  lifecycle: ManagedLifecycleState;
  bridgeVersion: string;
  testedCodexVersion: string;
  address: string | null;
  codexReady: boolean;
  connectedDevices: number;
  activeTasks: number;
  totalTasks: number;
  clearableTasks: number;
  pendingAttention: number;
  uptimeSeconds: number;
  lastError?: string;
}

export interface ManagementServerOptions {
  token: string;
  bridgeVersion: string;
  testedCodexVersion: string;
  deckServer: ControlDeckServer;
  tasks: TaskRegistry;
  approvals: ApprovalService;
  desktopInput: DesktopInputService;
  logger: StructuredLogger;
  reload: () => Promise<void>;
  shutdown: () => void;
}

function activeTaskCount(tasks: TaskRegistry): number {
  return tasks
    .all()
    .filter((task) =>
      [
        "starting",
        "running",
        "waiting_approval",
        "waiting_input",
        "stale",
      ].includes(task.status),
    ).length;
}

export class ManagementServer extends EventEmitter {
  private readonly app = Fastify({ logger: false });
  private readonly sockets = new WebSocketServer({
    noServer: true,
    maxPayload: 64 * 1024,
  });
  private startedAt = Date.now();

  constructor(private readonly options: ManagementServerOptions) {
    super();
    this.app.addHook("onRequest", async (request, reply) => {
      if (!this.authorized(request.headers.authorization))
        await reply.code(401).send({ error: "Unauthorized" });
    });
    this.app.get("/v1/status", async () => this.status());
    this.app.get("/v1/devices", async () => ({
      devices: this.options.deckServer.devices(),
    }));
    this.app.get("/v1/attention", async () => ({
      approvals: this.options.approvals.desktopList(),
      inputs: this.options.desktopInput.list(),
    }));
    this.app.get<{ Querystring: { limit?: string } }>(
      "/v1/logs",
      async (request) => ({
        entries: this.options.logger.recent(Number(request.query.limit ?? 200)),
      }),
    );
    this.app.post("/v1/reload", async (_request, reply) => {
      await this.options.reload();
      return reply.send({ reloaded: true });
    });
    this.app.post("/v1/tasks/clear-finished", async () => ({
      cleared: this.options.tasks.clearFinished().length,
    }));
    this.app.post("/v1/shutdown", async (_request, reply) => {
      await reply.code(202).send({ stopping: true });
      setImmediate(this.options.shutdown);
    });
    this.app.post<{ Params: { id: string }; Body: { answers?: unknown } }>(
      "/v1/input/:id/respond",
      async (request) => ({
        input: this.options.desktopInput.respond(
          request.params.id,
          request.body?.answers,
        ),
      }),
    );
    this.app.post<{ Params: { id: string }; Body: unknown }>(
      "/v1/approvals/:id/respond",
      async (request) => ({
        approval: this.options.approvals.respondDesktop(
          request.params.id,
          request.body,
        ),
      }),
    );
    this.app.server.on("upgrade", (request, socket, head) => {
      const url = new URL(request.url ?? "/", "http://127.0.0.1");
      if (
        url.pathname !== "/v1/events" ||
        !this.authorized(request.headers.authorization)
      ) {
        socket.destroy();
        return;
      }
      this.sockets.handleUpgrade(request, socket, head, (webSocket) =>
        this.sockets.emit("connection", webSocket),
      );
    });
    this.sockets.on("connection", (socket) => {
      socket.send(JSON.stringify({ type: "status", status: this.status() }));
      socket.send(
        JSON.stringify({
          type: "attention.snapshot",
          approvals: this.options.approvals.desktopList(),
          inputs: this.options.desktopInput.list(),
        }),
      );
    });

    options.tasks.on("upsert", (task: ManagedTask) =>
      this.broadcast({ type: "task", task }),
    );
    options.tasks.on("remove", (taskId: string) =>
      this.broadcast({ type: "task.remove", taskId }),
    );
    options.approvals.on("open", (approval) => {
      const desktopApproval = options.approvals
        .desktopList()
        .find((candidate) => candidate.approval.id === approval.id);
      this.broadcast({ type: "approval.open", approval: desktopApproval });
    });
    options.approvals.on("resolved", (result) =>
      this.broadcast({ type: "approval.resolved", ...result }),
    );
    options.desktopInput.on("waiting", (input) =>
      this.broadcast({ type: "input.open", input }),
    );
    options.desktopInput.on("resolved", (result) =>
      this.broadcast({ type: "input.resolved", ...result }),
    );
    options.deckServer.on("devices", (devices) =>
      this.broadcast({ type: "devices", devices }),
    );
    options.logger.on("entry", (entry) =>
      this.broadcast({ type: "log", entry }),
    );
  }

  async start(port = 0): Promise<number> {
    this.startedAt = Date.now();
    await this.app.listen({ host: "127.0.0.1", port });
    const address = this.app.server.address();
    if (!address || typeof address === "string")
      throw new Error("Management server did not bind to TCP loopback");
    return address.port;
  }

  async stop(): Promise<void> {
    for (const socket of this.sockets.clients) socket.close();
    this.sockets.close();
    await this.app.close();
  }

  status(): ManagedBridgeStatus {
    const health = this.options.deckServer.health();
    const lastError = this.options.logger.latestError();
    return {
      version: 1,
      lifecycle: health.status === "ok" ? "online" : "degraded",
      bridgeVersion: this.options.bridgeVersion,
      testedCodexVersion: this.options.testedCodexVersion,
      address: this.options.deckServer.address(),
      codexReady: health.codexReady,
      connectedDevices: health.connectedDevices,
      activeTasks: activeTaskCount(this.options.tasks),
      totalTasks: this.options.tasks.all().length,
      clearableTasks: this.options.tasks.clearableCount(),
      pendingAttention:
        this.options.approvals.list().length +
        this.options.desktopInput.list().length,
      uptimeSeconds: Math.floor((Date.now() - this.startedAt) / 1000),
      ...(lastError ? { lastError } : {}),
    };
  }

  private authorized(header: string | undefined): boolean {
    if (!header?.startsWith("Bearer ")) return false;
    const candidate = Buffer.from(header.slice(7));
    const expected = Buffer.from(this.options.token);
    return (
      candidate.length === expected.length &&
      timingSafeEqual(candidate, expected)
    );
  }

  private broadcast(event: Record<string, unknown>): void {
    const frame = JSON.stringify(event);
    for (const socket of this.sockets.clients)
      if (socket.readyState === socket.OPEN) socket.send(frame);
  }
}
