import process from "node:process";
import { pathToFileURL } from "node:url";

import { ApprovalService } from "./approvals/approval-service.js";
import { AppServerClient } from "./codex/app-server-client.js";
import { EventNormalizer } from "./codex/event-normalizer.js";
import type { AppServerNotification, AppServerServerRequest } from "./codex/types.js";
import { loadConfig } from "./config.js";
import { DesktopInputService } from "./desktop-input-service.js";
import { selectBindHost } from "./network/private-address.js";
import { StateStore } from "./persistence/state-store.js";
import { MessageRouter } from "./protocol/message-router.js";
import { ControlDeckServer } from "./server.js";
import { TaskRegistry } from "./tasks/task-registry.js";
import { WorkflowRegistry } from "./workflows/workflow-registry.js";
import { WorkflowRunner } from "./workflows/workflow-runner.js";

export const BRIDGE_VERSION = "0.1.0" as const;
export const TESTED_CODEX_VERSION = "0.140.0" as const;

function log(level: "info" | "warn" | "error", component: string, event: string, fields = {}): void {
  process.stdout.write(`${JSON.stringify({ timestamp: new Date().toISOString(), level, component, event, ...fields })}\n`);
}

export async function runBridge(configPath: string): Promise<() => Promise<void>> {
  const config = await loadConfig(configPath);
  const workflows = new WorkflowRegistry();
  await workflows.load(config.workflowsFile);
  const tasks = new TaskRegistry();
  const store = new StateStore(config.stateFile);
  tasks.restore((await store.load()).tasks);
  tasks.markActiveStale();

  const appServer = new AppServerClient({
    codexPath: config.codexPath,
    clientVersion: BRIDGE_VERSION,
    autoRestart: true,
  });
  const normalizer = new EventNormalizer(tasks);
  const approvals = new ApprovalService(tasks);
  const desktopInput = new DesktopInputService(tasks);
  const runner = new WorkflowRunner(appServer, workflows, tasks);
  const router = new MessageRouter(tasks, workflows, runner, approvals);
  const host = config.bindHost ?? selectBindHost();
  const server = new ControlDeckServer(BRIDGE_VERSION, tasks, approvals, router, () => appServer.ready);

  let saveTimer: NodeJS.Timeout | undefined;
  tasks.on("upsert", () => {
    if (saveTimer) clearTimeout(saveTimer);
    saveTimer = setTimeout(() => {
      void store.save({ version: 1, tasks: tasks.all(), display: {} }).catch((error: unknown) =>
        log("error", "persistence", "save_failed", { error: error instanceof Error ? error.message : String(error) }),
      );
    }, 100);
  });

  appServer.on("notification", (notification: AppServerNotification) => normalizer.handle(notification));
  appServer.on("request", (request: AppServerServerRequest) => {
    if (request.method === "item/tool/requestUserInput") desktopInput.handle(request);
    else if (request.method.includes("requestApproval")) approvals.open(request);
    else request.reject(-32601, "Unsupported App Server request");
  });
  appServer.on("stderr", (message: string) => log("warn", "app-server", "stderr", { message: message.trim().slice(0, 500) }));
  appServer.on("exit", (result: unknown) => {
    tasks.markActiveStale();
    log("warn", "app-server", "exited", { result });
  });
  appServer.on("ready", () => {
    void resumeManagedTasks(appServer, tasks);
  });

  await appServer.start();
  await server.start(host, config.port);
  log("warn", "network", "trusted_lan_only", {
    message: "Unauthenticated device protocol. Do not expose through public listeners, tunnels, or port forwarding.",
    host,
    port: config.port,
  });
  log("info", "bridge", "ready", { address: server.address(), testedCodexVersion: TESTED_CODEX_VERSION });

  process.on("SIGHUP", () => {
    void workflows.load(config.workflowsFile).then(
      () => log("info", "workflows", "reloaded"),
      (error: unknown) => log("error", "workflows", "reload_failed", { error: error instanceof Error ? error.message : String(error) }),
    );
  });

  return async () => {
    if (saveTimer) clearTimeout(saveTimer);
    await store.save({ version: 1, tasks: tasks.all(), display: {} });
    await server.stop();
    appServer.stop();
  };
}

async function resumeManagedTasks(appServer: AppServerClient, tasks: TaskRegistry): Promise<void> {
  for (const task of tasks.all().filter((candidate) => candidate.status === "stale")) {
    try {
      await appServer.resumeThread(task.threadId);
      const restore = task.previousActiveStatus;
      const status = restore && !["completed", "failed", "cancelled", "stale"].includes(restore) ? restore : "running";
      tasks.transition(task.id, status, "Codex thread resumed");
    } catch (error) {
      tasks.transition(task.id, "failed", error instanceof Error ? error.message : "Thread resume failed");
    }
  }
}

async function main(): Promise<void> {
  const configPath = process.env.CODEXDECK_CONFIG ?? process.argv[2] ?? "apps/bridge/config/bridge.example.yaml";
  const stop = await runBridge(configPath);
  const shutdown = () => {
    void stop().finally(() => process.exit(0));
  };
  process.once("SIGINT", shutdown);
  process.once("SIGTERM", shutdown);
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  main().catch((error: unknown) => {
    log("error", "bridge", "startup_failed", { error: error instanceof Error ? error.message : String(error) });
    process.exitCode = 1;
  });
}
