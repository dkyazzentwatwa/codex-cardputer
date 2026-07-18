import process from "node:process";
import { pathToFileURL } from "node:url";

import type { UsageUpdate } from "@codexdeck/protocol";

import { ApprovalService } from "./approvals/approval-service.js";
import { AppServerClient } from "./codex/app-server-client.js";
import { EventNormalizer } from "./codex/event-normalizer.js";
import { ExternalSessionMonitor } from "./codex/external-session-monitor.js";
import type {
  AppServerNotification,
  AppServerServerRequest,
} from "./codex/types.js";
import { loadConfig } from "./config.js";
import { DesktopInputService } from "./desktop-input-service.js";
import { StructuredLogger } from "./logging/structured-logger.js";
import { ManagementServer } from "./management/management-server.js";
import { selectBindHost } from "./network/private-address.js";
import { StateStore } from "./persistence/state-store.js";
import { MessageRouter } from "./protocol/message-router.js";
import { ControlDeckServer } from "./server.js";
import { TaskRegistry } from "./tasks/task-registry.js";
import { WorkflowRegistry } from "./workflows/workflow-registry.js";
import { WorkflowRunner } from "./workflows/workflow-runner.js";
import { discoverSkillMacros } from "./workflows/skill-macros.js";

export const BRIDGE_VERSION = "0.1.0" as const;
export const TESTED_CODEX_VERSION = "0.140.0" as const;
let activeLogger: StructuredLogger | undefined;

function log(
  level: "info" | "warn" | "error",
  component: string,
  event: string,
  fields = {},
): void {
  if (activeLogger) {
    activeLogger.log(level, component, event, fields);
    return;
  }
  process.stdout.write(
    `${JSON.stringify({ timestamp: new Date().toISOString(), level, component, event, ...fields })}\n`,
  );
}

export async function runBridge(
  configPath: string,
): Promise<() => Promise<void>> {
  const config = await loadConfig(configPath);
  const logger = new StructuredLogger(config.logsDirectory, config.prettyLogs);
  activeLogger = logger;
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
  const externalSessions = new ExternalSessionMonitor(appServer, tasks, {
    enabled: config.externalSessions.enabled,
    limit: config.externalSessions.maxSessions,
    cwd: config.externalSessions.cwd,
    sourceKinds: config.externalSessions.sourceKinds,
  });
  const normalizer = new EventNormalizer(tasks);
  const approvals = new ApprovalService(tasks);
  const desktopInput = new DesktopInputService(tasks);
  const runner = new WorkflowRunner(appServer, workflows, tasks);
  const router = new MessageRouter(tasks, workflows, runner, approvals);
  const host =
    config.bindMode === "loopback"
      ? "127.0.0.1"
      : config.bindMode === "explicit"
        ? config.bindHost!
        : (config.bindHost ?? selectBindHost());
  const server = new ControlDeckServer(
    BRIDGE_VERSION,
    tasks,
    approvals,
    router,
    () => appServer.ready,
  );

  let saveTimer: NodeJS.Timeout | undefined;
  const scheduleSave = () => {
    if (saveTimer) clearTimeout(saveTimer);
    saveTimer = setTimeout(() => {
      void store
        .save({
          version: 1,
          tasks: tasks.all().filter((task) => !task.external),
          display: {},
        })
        .catch((error: unknown) =>
          log("error", "persistence", "save_failed", {
            error: error instanceof Error ? error.message : String(error),
          }),
        );
    }, 100);
  };
  tasks.on("upsert", scheduleSave);
  tasks.on("remove", scheduleSave);

  appServer.on("notification", (notification: AppServerNotification) => {
    normalizer.handle(notification);
    if (notification.method === "account/rateLimits/updated")
      void refreshUsage(appServer, server);
  });
  appServer.on("request", (request: AppServerServerRequest) => {
    if (request.method === "item/tool/requestUserInput")
      desktopInput.handle(request);
    else if (request.method.includes("requestApproval"))
      approvals.open(request);
    else request.reject(-32601, "Unsupported App Server request");
  });
  appServer.on("stderr", (message: string) =>
    log("warn", "app-server", "stderr", {
      message: message.trim().slice(0, 500),
    }),
  );
  appServer.on("exit", (result: unknown) => {
    for (const task of tasks.all()) {
      if (task.external) tasks.remove(task.id);
    }
    tasks.markActiveStale();
    log("warn", "app-server", "exited", { result });
  });
  let initialStart = true;
  appServer.on("ready", () => {
    if (initialStart) return;
    void Promise.all([
      resumeManagedTasks(appServer, tasks),
      refreshSkillMacros(appServer, workflows, router),
      refreshUsage(appServer, server),
      externalSessions.refresh(),
    ]).then(() => server.broadcastMacros());
  });

  await appServer.start();
  initialStart = false;
  await refreshSkillMacros(appServer, workflows, router);
  await refreshUsage(appServer, server);
  await resumeManagedTasks(appServer, tasks);
  await externalSessions.refresh();
  await server.start(host, config.port);
  const usageTimer = setInterval(
    () => void refreshUsage(appServer, server),
    60_000,
  );
  usageTimer.unref();
  const externalSessionsTimer = setInterval(
    () =>
      void externalSessions.refresh().catch((error: unknown) =>
        log("warn", "external-sessions", "refresh_failed", {
          error: error instanceof Error ? error.message : String(error),
        }),
      ),
    config.externalSessions.pollSeconds * 1_000,
  );
  externalSessionsTimer.unref();

  const reloadWorkflows = async (): Promise<void> => {
    await workflows.load(config.workflowsFile);
    await refreshSkillMacros(appServer, workflows, router);
    server.broadcastMacros();
    log("info", "workflows", "reloaded");
  };

  let management: ManagementServer | undefined;
  let stopping = false;
  const stopBridge = async (): Promise<void> => {
    if (stopping) return;
    stopping = true;
    process.off("SIGHUP", handleReload);
    clearInterval(usageTimer);
    clearInterval(externalSessionsTimer);
    if (saveTimer) clearTimeout(saveTimer);
    await store.save({
      version: 1,
      tasks: tasks.all().filter((task) => !task.external),
      display: {},
    });
    await server.stop();
    appServer.stop();
    if (management) await management.stop();
    await logger.flush();
    if (activeLogger === logger) activeLogger = undefined;
  };

  const handleReload = (): void => {
    void reloadWorkflows().catch((error: unknown) =>
      log("error", "workflows", "reload_failed", {
        error: error instanceof Error ? error.message : String(error),
      }),
    );
  };

  const managementToken = process.env.CODEXDECK_ADMIN_TOKEN;
  if (managementToken) {
    if (managementToken.length < 32)
      throw new Error(
        "CODEXDECK_ADMIN_TOKEN must contain at least 32 characters",
      );
    management = new ManagementServer({
      token: managementToken,
      bridgeVersion: BRIDGE_VERSION,
      testedCodexVersion: TESTED_CODEX_VERSION,
      deckServer: server,
      tasks,
      approvals,
      desktopInput,
      logger,
      reload: reloadWorkflows,
      shutdown: () => {
        void stopBridge().finally(() => process.exit(0));
      },
    });
    const requestedPort = Number(process.env.CODEXDECK_ADMIN_PORT ?? 0);
    const managementPort = await management.start(
      Number.isInteger(requestedPort) && requestedPort >= 0 ? requestedPort : 0,
    );
    log("info", "management", "ready", {
      host: "127.0.0.1",
      port: managementPort,
    });
  }
  log("warn", "network", "trusted_lan_only", {
    message:
      "Unauthenticated device protocol. Do not expose through public listeners, tunnels, or port forwarding.",
    host,
    port: config.port,
  });
  log("info", "bridge", "ready", {
    address: server.address(),
    testedCodexVersion: TESTED_CODEX_VERSION,
  });

  process.on("SIGHUP", handleReload);

  return stopBridge;
}

async function refreshUsage(
  appServer: AppServerClient,
  server: ControlDeckServer,
): Promise<void> {
  try {
    const response = await appServer.readRateLimits();
    const bucket = response.rateLimitsByLimitId?.codex ?? response.rateLimits;
    const remaining = (usedPercent: number): number =>
      Math.max(0, Math.min(100, 100 - usedPercent));
    const usage: UsageUpdate = {
      type: "usage.update",
      available: Boolean(bucket.primary || bucket.secondary),
      ...(bucket.limitName ? { limitName: bucket.limitName } : {}),
      ...(bucket.primary
        ? {
            primaryRemainingPercent: remaining(bucket.primary.usedPercent),
            ...(bucket.primary.resetsAt == null
              ? {}
              : { primaryResetsAt: bucket.primary.resetsAt }),
            ...(bucket.primary.windowDurationMins == null
              ? {}
              : { primaryWindowMinutes: bucket.primary.windowDurationMins }),
          }
        : {}),
      ...(bucket.secondary
        ? {
            secondaryRemainingPercent: remaining(bucket.secondary.usedPercent),
            ...(bucket.secondary.resetsAt == null
              ? {}
              : { secondaryResetsAt: bucket.secondary.resetsAt }),
            ...(bucket.secondary.windowDurationMins == null
              ? {}
              : {
                  secondaryWindowMinutes: bucket.secondary.windowDurationMins,
                }),
          }
        : {}),
    };
    server.setUsage(usage);
  } catch (error) {
    server.setUsage({ type: "usage.update", available: false });
    log("warn", "usage", "refresh_failed", {
      error: error instanceof Error ? error.message : String(error),
    });
  }
}

async function refreshSkillMacros(
  appServer: AppServerClient,
  workflows: WorkflowRegistry,
  router: MessageRouter,
): Promise<void> {
  const macros = await discoverSkillMacros(
    appServer,
    workflows,
    (projectId, error) =>
      log("warn", "skills", "discovery_failed", {
        projectId,
        error: error instanceof Error ? error.message : String(error),
      }),
  );
  router.setSkillMacros(macros);
  log("info", "skills", "discovered", { count: macros.length });
}

async function resumeManagedTasks(
  appServer: AppServerClient,
  tasks: TaskRegistry,
): Promise<void> {
  for (const task of tasks
    .all()
    .filter((candidate) => candidate.status === "stale")) {
    try {
      await appServer.resumeThread(task.threadId);
      const restore = task.previousActiveStatus;
      const status =
        restore &&
        !["completed", "failed", "cancelled", "stale"].includes(restore)
          ? restore
          : "running";
      tasks.transition(task.id, status, "Codex thread resumed");
    } catch (error) {
      tasks.transition(
        task.id,
        "failed",
        error instanceof Error ? error.message : "Thread resume failed",
      );
    }
  }
}

async function main(): Promise<void> {
  const configPath =
    process.env.CODEXDECK_CONFIG ??
    process.argv[2] ??
    "apps/bridge/config/bridge.example.yaml";
  const stop = await runBridge(configPath);
  const shutdown = () => {
    void stop().finally(() => process.exit(0));
  };
  process.once("SIGINT", shutdown);
  process.once("SIGTERM", shutdown);
}

if (
  process.argv[1] &&
  import.meta.url === pathToFileURL(process.argv[1]).href
) {
  main().catch((error: unknown) => {
    log("error", "bridge", "startup_failed", {
      error: error instanceof Error ? error.message : String(error),
    });
    process.exitCode = 1;
  });
}
