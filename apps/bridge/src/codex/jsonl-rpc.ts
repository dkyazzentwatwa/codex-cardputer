import { EventEmitter } from "node:events";
import type { Readable, Writable } from "node:stream";

import type {
  AppServerNotification,
  AppServerServerRequest,
  RpcErrorShape,
  RpcId,
} from "./types.js";

interface PendingRequest {
  resolve(value: unknown): void;
  reject(error: Error): void;
}

interface RpcMessage {
  id?: RpcId;
  method?: string;
  params?: unknown;
  result?: unknown;
  error?: RpcErrorShape;
}

export class RpcRemoteError extends Error {
  constructor(
    readonly code: number,
    message: string,
    readonly data?: unknown,
  ) {
    super(message);
    this.name = "RpcRemoteError";
  }
}

export class JsonlRpcTransport extends EventEmitter {
  private buffer = "";
  private nextId = 1;
  private readonly pending = new Map<RpcId, PendingRequest>();
  private closed = false;

  constructor(
    private readonly input: Writable,
    output: Readable,
  ) {
    super();
    output.on("data", (chunk: Buffer | string) => this.ingest(chunk.toString()));
    output.on("end", () => this.close(new Error("App Server stdout ended")));
    output.on("error", (error) => this.close(error));
    input.on("error", (error) => this.close(error));
  }

  request<T = unknown>(method: string, params: unknown): Promise<T> {
    if (this.closed) return Promise.reject(new Error("JSONL RPC transport is closed"));
    const id = this.nextId++;
    return new Promise<T>((resolve, reject) => {
      this.pending.set(id, { resolve: (value) => resolve(value as T), reject });
      this.write({ id, method, params });
    });
  }

  notify(method: string, params: unknown): void {
    if (this.closed) throw new Error("JSONL RPC transport is closed");
    this.write({ method, params });
  }

  close(reason = new Error("JSONL RPC transport closed")): void {
    if (this.closed) return;
    this.closed = true;
    for (const pending of this.pending.values()) pending.reject(reason);
    this.pending.clear();
    this.emit("close", reason);
  }

  private write(message: RpcMessage): void {
    this.input.write(`${JSON.stringify(message)}\n`);
  }

  private ingest(chunk: string): void {
    this.buffer += chunk;
    while (true) {
      const newline = this.buffer.indexOf("\n");
      if (newline < 0) break;
      const line = this.buffer.slice(0, newline).trim();
      this.buffer = this.buffer.slice(newline + 1);
      if (line) this.handleLine(line);
    }
  }

  private handleLine(line: string): void {
    let message: RpcMessage;
    try {
      message = JSON.parse(line) as RpcMessage;
    } catch {
      this.emit("malformed", line);
      return;
    }

    if (message.method && message.id !== undefined) {
      const id = message.id;
      const request: AppServerServerRequest = {
        id,
        method: message.method,
        params: message.params,
        respond: (result) => this.write({ id, result }),
        reject: (code, errorMessage) =>
          this.write({ id, error: { code, message: errorMessage } }),
      };
      this.emit("request", request);
      return;
    }

    if (message.method) {
      const notification: AppServerNotification = {
        method: message.method,
        params: message.params,
      };
      this.emit("notification", notification);
      return;
    }

    if (message.id === undefined) {
      this.emit("malformed", line);
      return;
    }
    const pending = this.pending.get(message.id);
    if (!pending) return;
    this.pending.delete(message.id);
    if (message.error) {
      pending.reject(new RpcRemoteError(message.error.code, message.error.message, message.error.data));
    } else {
      pending.resolve(message.result);
    }
  }
}
