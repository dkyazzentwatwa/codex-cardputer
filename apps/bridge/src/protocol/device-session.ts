import { randomUUID } from "node:crypto";

import type { WebSocket } from "ws";

import {
  HEARTBEAT_SECONDS,
  PROTOCOL_VERSION,
  STALE_AFTER_SECONDS,
  encodeServerMessage,
  parseDeviceFrame,
} from "@codexdeck/protocol";
import type { ServerMessage } from "@codexdeck/protocol";

import type { ApprovalService } from "../approvals/approval-service.js";
import type { TaskRegistry } from "../tasks/task-registry.js";
import type { MessageRouter } from "./message-router.js";

export class DeviceSession {
  private deviceId: string | undefined;
  private lastSeen = Date.now();
  private readonly heartbeat: NodeJS.Timeout;

  constructor(
    private readonly socket: WebSocket,
    private readonly bridgeVersion: string,
    private readonly tasks: TaskRegistry,
    private readonly approvals: ApprovalService,
    private readonly router: MessageRouter,
    onClose: () => void,
  ) {
    socket.on("message", (data, isBinary) => {
      if (isBinary) {
        socket.close(1003, "JSON text frames only");
        return;
      }
      void this.receive(data.toString());
    });
    let closed = false;
    const handleClose = () => {
      if (closed) return;
      closed = true;
      clearInterval(this.heartbeat);
      onClose();
    };
    socket.on("close", handleClose);
    socket.on("error", handleClose);
    this.heartbeat = setInterval(() => this.tick(), HEARTBEAT_SECONDS * 1000);
    this.heartbeat.unref();
  }

  send(message: ServerMessage): void {
    if (this.socket.readyState === this.socket.OPEN)
      this.socket.send(encodeServerMessage(message));
  }

  close(): void {
    clearInterval(this.heartbeat);
    if (this.socket.readyState === this.socket.OPEN) this.socket.close();
  }

  private async receive(frame: string): Promise<void> {
    this.lastSeen = Date.now();
    try {
      const message = parseDeviceFrame(frame);
      if (!this.deviceId) {
        if (message.type !== "hello") {
          this.send({
            type: "error",
            code: "HANDSHAKE_REQUIRED",
            message: "Send hello first",
            recoverable: false,
          });
          this.socket.close(1008, "Handshake required");
          return;
        }
        this.deviceId = message.deviceId;
        this.send({
          type: "welcome",
          protocol: PROTOCOL_VERSION,
          bridgeVersion: this.bridgeVersion,
          serverTime: new Date().toISOString(),
          heartbeatSeconds: HEARTBEAT_SECONDS,
        });
        this.send({
          type: "task.snapshot",
          tasks: this.tasks.all().slice(0, 20),
        });
        this.send({
          type: "macro.snapshot",
          macros: this.router.macroSnapshot(),
        });
        for (const approval of this.approvals.list())
          this.send({ type: "approval.open", approval });
        return;
      }
      if (message.type === "hello") return;
      for (const response of await this.router.handle(this.deviceId, message))
        this.send(response);
    } catch (error) {
      const code =
        error instanceof Error && "code" in error
          ? String(error.code)
          : "INVALID_MESSAGE";
      this.send({
        type: "error",
        code: /^[A-Z][A-Z0-9_]+$/.test(code) ? code : "INVALID_MESSAGE",
        message:
          error instanceof Error
            ? error.message.slice(0, 160)
            : "Invalid message",
        recoverable: true,
      });
    }
  }

  private tick(): void {
    if (Date.now() - this.lastSeen >= STALE_AFTER_SECONDS * 1000) {
      this.socket.terminate();
      clearInterval(this.heartbeat);
      return;
    }
    this.send({ type: "ping", nonce: randomUUID() });
  }
}
