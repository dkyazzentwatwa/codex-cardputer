import { spawn } from "node:child_process";
import { EventEmitter } from "node:events";
import type { Readable, Writable } from "node:stream";

import { JsonlRpcTransport } from "./jsonl-rpc.js";
import type {
  AppServerNotification,
  AppServerServerRequest,
  AppServerThread,
  AppServerTurn,
  AppServerUserInput,
  SkillMetadata,
} from "./types.js";

interface AppServerProcess {
  stdin: Writable;
  stdout: Readable;
  stderr: Readable;
  once(event: "exit", listener: (code: number | null, signal: NodeJS.Signals | null) => void): this;
  kill(signal?: NodeJS.Signals): boolean;
}

export type SpawnAppServer = () => AppServerProcess;

export interface AppServerClientOptions {
  codexPath?: string;
  clientVersion: string;
  autoRestart?: boolean;
  spawnProcess?: SpawnAppServer;
}

export class AppServerClient extends EventEmitter {
  private process: AppServerProcess | undefined;
  private transport: JsonlRpcTransport | undefined;
  private stopping = false;
  private restartAttempt = 0;
  private restartTimer: NodeJS.Timeout | undefined;

  constructor(private readonly options: AppServerClientOptions) {
    super();
  }

  get ready(): boolean {
    return this.transport !== undefined;
  }

  async start(): Promise<void> {
    if (this.transport) return;
    this.stopping = false;
    const process = (this.options.spawnProcess ?? this.defaultSpawn)();
    this.process = process;
    const transport = new JsonlRpcTransport(process.stdin, process.stdout);
    this.transport = transport;
    transport.on("notification", (notification: AppServerNotification) =>
      this.emit("notification", notification),
    );
    transport.on("request", (request: AppServerServerRequest) => this.emit("request", request));
    transport.on("malformed", (line: string) => this.emit("malformed", line));
    process.stderr.on("data", (chunk: Buffer | string) => this.emit("stderr", chunk.toString()));
    process.once("exit", (code, signal) => this.handleExit(code, signal));

    try {
      await transport.request("initialize", {
        clientInfo: {
          name: "cardputer_codex_control",
          title: "CardPuter Codex Control Deck",
          version: this.options.clientVersion,
        },
      });
      transport.notify("initialized", {});
      this.restartAttempt = 0;
      this.emit("ready");
    } catch (error) {
      this.transport = undefined;
      process.kill("SIGTERM");
      throw error;
    }
  }

  stop(): void {
    this.stopping = true;
    if (this.restartTimer) clearTimeout(this.restartTimer);
    this.restartTimer = undefined;
    this.transport?.close(new Error("App Server stopped"));
    this.transport = undefined;
    this.process?.kill("SIGTERM");
    this.process = undefined;
  }

  async startThread(params: {
    cwd: string;
    model?: string;
    approvalPolicy?: "untrusted" | "on-request" | "never";
    sandbox?: "read-only" | "workspace-write" | "danger-full-access";
  }): Promise<AppServerThread> {
    const response = await this.rpc<{ thread: AppServerThread }>("thread/start", params);
    return response.thread;
  }

  async resumeThread(threadId: string): Promise<AppServerThread> {
    const response = await this.rpc<{ thread: AppServerThread }>("thread/resume", { threadId });
    return response.thread;
  }

  async startTurn(threadId: string, input: AppServerUserInput[]): Promise<AppServerTurn> {
    const response = await this.rpc<{ turn: AppServerTurn }>("turn/start", { threadId, input });
    return response.turn;
  }

  async steerTurn(threadId: string, expectedTurnId: string, input: AppServerUserInput[]): Promise<void> {
    await this.rpc("turn/steer", { threadId, expectedTurnId, input });
  }

  async interruptTurn(threadId: string, turnId: string): Promise<void> {
    await this.rpc("turn/interrupt", { threadId, turnId });
  }

  async listSkills(cwd: string): Promise<SkillMetadata[]> {
    const response = await this.rpc<{
      data: Array<{ skills: SkillMetadata[] }>;
    }>("skills/list", { cwds: [cwd], forceReload: false });
    return response.data.flatMap((entry) => entry.skills).filter((skill) => skill.enabled);
  }

  private rpc<T = unknown>(method: string, params: unknown): Promise<T> {
    if (!this.transport) return Promise.reject(new Error("Codex App Server is not ready"));
    return this.transport.request<T>(method, params);
  }

  private readonly defaultSpawn = (): AppServerProcess =>
    spawn(this.options.codexPath ?? "codex", ["app-server", "--listen", "stdio://"], {
      stdio: ["pipe", "pipe", "pipe"],
      env: process.env,
    });

  private handleExit(code: number | null, signal: NodeJS.Signals | null): void {
    this.transport?.close(new Error(`App Server exited (${String(code ?? signal)})`));
    this.transport = undefined;
    this.process = undefined;
    this.emit("exit", { code, signal });
    if (this.stopping || !this.options.autoRestart) return;
    const delay = Math.min(30_000, 500 * 2 ** this.restartAttempt++);
    this.restartTimer = setTimeout(() => {
      void this.start().catch((error: unknown) => {
        this.emit("restartError", error);
        this.handleExit(null, null);
      });
    }, delay);
    this.restartTimer.unref();
  }
}
