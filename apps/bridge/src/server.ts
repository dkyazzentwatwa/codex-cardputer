import { EventEmitter } from "node:events";

import Fastify from "fastify";
import { WebSocketServer } from "ws";

import { DEVICE_PATH, MAX_FRAME_BYTES } from "@codexdeck/protocol";
import type { ApprovalRequest, ServerMessage } from "@codexdeck/protocol";
import type { UsageUpdate } from "@codexdeck/protocol";

import type { ApprovalService } from "./approvals/approval-service.js";
import { MdnsAdvertiser } from "./discovery/mdns.js";
import { DeviceSession } from "./protocol/device-session.js";
import type { ConnectedDevice } from "./protocol/device-session.js";
import type { MessageRouter } from "./protocol/message-router.js";
import {
  toTaskSummary,
  type ManagedTask,
  type TaskRegistry,
} from "./tasks/task-registry.js";

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
  private usage: UsageUpdate | undefined;

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
          this.emit("devices", this.devices());
        },
        (device) => {
          for (const existing of this.sessions) {
            if (
              existing !== session &&
              existing.info()?.deviceId === device.deviceId
            ) {
              this.sessions.delete(existing);
              existing.close();
            }
          }
          this.emit("devices", this.devices());
        },
        () => this.usage,
      );
      this.sessions.add(session);
      this.emit("devices", this.devices());
    });
    tasks.on("upsert", (task: ManagedTask) =>
      this.broadcast({ type: "task.upsert", task: toTaskSummary(task) }),
    );
    tasks.on("remove", (taskId: string) =>
      this.broadcast({ type: "task.remove", taskId }),
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
      connectedDevices: this.devices().length,
      activeTasks,
      uptimeSeconds: Math.floor((Date.now() - this.startedAt) / 1000),
    };
  }

  devices(): ConnectedDevice[] {
    const byDeviceId = new Map<string, ConnectedDevice>();
    for (const session of this.sessions) {
      const device = session.info();
      if (device) byDeviceId.set(device.deviceId, device);
    }
    return [...byDeviceId.values()].sort((left, right) =>
      left.deviceName.localeCompare(right.deviceName),
    );
  }

  broadcastMacros(): void {
    this.broadcast({
      type: "macro.snapshot",
      macros: this.router.macroSnapshot(),
    });
  }

  setUsage(usage: UsageUpdate): void {
    this.usage = usage;
    this.broadcast(usage);
  }

  private broadcast(message: ServerMessage): void {
    for (const session of this.sessions) session.send(message);
  }
}
