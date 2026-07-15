import { EventEmitter } from "node:events";

import Fastify from "fastify";
import { WebSocketServer } from "ws";

import { DEVICE_PATH, MAX_FRAME_BYTES } from "@codexdeck/protocol";
import type { ApprovalRequest, ServerMessage } from "@codexdeck/protocol";

import type { ApprovalService } from "./approvals/approval-service.js";
import { MdnsAdvertiser } from "./discovery/mdns.js";
import { DeviceSession } from "./protocol/device-session.js";
import type { MessageRouter } from "./protocol/message-router.js";
import type { ManagedTask, TaskRegistry } from "./tasks/task-registry.js";

export interface BridgeHealth {
  status: "ok" | "degraded";
  bridgeVersion: string;
  codexReady: boolean;
  connectedDevices: number;
  activeTasks: number;
  uptimeSeconds: number;
}

export class ControlDeckServer extends EventEmitter {
  private readonly app = Fastify({ logger: false });
  private readonly sockets = new WebSocketServer({
    noServer: true,
    maxPayload: MAX_FRAME_BYTES,
  });
  private readonly sessions = new Set<DeviceSession>();
  private readonly mdns = new MdnsAdvertiser();
  private readonly startedAt = Date.now();
  private listening = false;

  constructor(
    private readonly bridgeVersion: string,
    private readonly tasks: TaskRegistry,
    approvals: ApprovalService,
    private readonly router: MessageRouter,
    private readonly codexReady: () => boolean,
  ) {
    super();
    this.app.get("/healthz", async () => this.health());
    this.app.server.on("upgrade", (request, socket, head) => {
      const url = new URL(request.url ?? "/", "http://localhost");
      if (url.pathname !== DEVICE_PATH) {
        socket.destroy();
        return;
      }
      this.sockets.handleUpgrade(request, socket, head, (webSocket) =>
        this.sockets.emit("connection", webSocket),
      );
    });
    this.sockets.on("connection", (socket) => {
      const session = new DeviceSession(
        socket,
        bridgeVersion,
        tasks,
        approvals,
        router,
        () => {
          this.sessions.delete(session);
        },
      );
      this.sessions.add(session);
    });
    tasks.on("upsert", (task: ManagedTask) =>
      this.broadcast({ type: "task.upsert", task }),
    );
    approvals.on("open", (approval: ApprovalRequest) =>
      this.broadcast({ type: "approval.open", approval }),
    );
    approvals.on(
      "resolved",
      (result: {
        approvalId: string;
        decision: "accept" | "decline" | "cancel";
      }) => this.broadcast({ type: "approval.resolved", ...result }),
    );
  }

  async start(host: string, port: number, advertise = true): Promise<void> {
    await this.app.listen({ host, port });
    this.listening = true;
    const address = this.app.server.address();
    const actualPort =
      typeof address === "object" && address ? address.port : port;
    if (advertise) this.mdns.start(actualPort, host, this.bridgeVersion);
    this.emit("listening", { host, port: actualPort });
  }

  async stop(): Promise<void> {
    if (!this.listening) return;
    this.mdns.stop();
    for (const session of this.sessions) session.close();
    this.sessions.clear();
    this.sockets.close();
    await this.app.close();
    this.listening = false;
  }

  address(): string | null {
    const address = this.app.server.address();
    if (!address) return null;
    return typeof address === "string"
      ? address
      : `${address.address}:${address.port}`;
  }

  health(): BridgeHealth {
    const codexReady = this.codexReady();
    const activeTasks = this.tasks
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
    return {
      status: codexReady ? "ok" : "degraded",
      bridgeVersion: this.bridgeVersion,
      codexReady,
      connectedDevices: this.sessions.size,
      activeTasks,
      uptimeSeconds: Math.floor((Date.now() - this.startedAt) / 1000),
    };
  }

  broadcastMacros(): void {
    this.broadcast({
      type: "macro.snapshot",
      macros: this.router.macroSnapshot(),
    });
  }

  private broadcast(message: ServerMessage): void {
    for (const session of this.sessions) session.send(message);
  }
}
