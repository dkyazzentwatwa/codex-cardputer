import type { TaskStatus } from "@codexdeck/protocol";

import type { AppServerThread } from "./types.js";
import type { AppServerClient } from "./app-server-client.js";
import { sanitizeSummary } from "../tasks/summary-builder.js";
import type { TaskRegistry } from "../tasks/task-registry.js";

export interface ExternalSessionMonitorOptions {
  enabled: boolean;
  limit: number;
  cwd: string[];
  sourceKinds: string[];
}

function externalTaskId(threadId: string): string {
  return `external-${threadId}`.slice(0, 128);
}

function timestamp(seconds: number | undefined): string {
  if (!seconds) return new Date().toISOString();
  const value = new Date(seconds * 1000);
  return Number.isNaN(value.getTime()) ? new Date().toISOString() : value.toISOString();
}

function statusFor(thread: AppServerThread): TaskStatus | undefined {
  if (thread.status?.type !== "active") return undefined;
  const flags = new Set(thread.status.activeFlags ?? []);
  if (flags.has("waitingOnApproval")) return "waiting_approval";
  if (flags.has("waitingOnUserInput")) return "waiting_input";
  return "running";
}

function sourceLabel(source: string | undefined): string {
  switch (source) {
    case "cli":
      return "Codex CLI";
    case "vscode":
      return "Codex VS Code";
    case "exec":
      return "Codex Exec";
    case "appServer":
      return "Codex App";
    default:
      return "Codex session";
  }
}

function summaryFor(thread: AppServerThread, status: TaskStatus): string {
  if (status === "waiting_approval") return "External session needs approval on desktop";
  if (status === "waiting_input") return "External session needs input on desktop";
  return sanitizeSummary(`${sourceLabel(thread.source)} active`, 64);
}

/**
 * Mirrors active local Codex threads into the deck as read-only tasks. These
 * entries deliberately do not persist: the App Server remains the source of
 * truth and a restarted bridge re-discovers the current active sessions.
 */
export class ExternalSessionMonitor {
  private refreshInFlight: Promise<void> | undefined;

  constructor(
    private readonly appServer: AppServerClient,
    private readonly tasks: TaskRegistry,
    private readonly options: ExternalSessionMonitorOptions,
  ) {}

  refresh(): Promise<void> {
    if (!this.options.enabled || !this.appServer.ready) return Promise.resolve();
    if (this.refreshInFlight) return this.refreshInFlight;
    this.refreshInFlight = this.refreshNow().finally(() => {
      this.refreshInFlight = undefined;
    });
    return this.refreshInFlight;
  }

  private async refreshNow(): Promise<void> {
    const threads = await this.appServer.listThreads({
      limit: this.options.limit,
      cwd: this.options.cwd,
      sourceKinds: this.options.sourceKinds,
    });
    const activeThreadIds = new Set<string>();
    for (const thread of threads) {
      const status = statusFor(thread);
      if (!status) continue;
      const managed = this.tasks.byThread(thread.id);
      if (managed && !managed.external) continue;
      activeThreadIds.add(thread.id);
      const taskId = managed?.id ?? externalTaskId(thread.id);
      const title = sanitizeSummary(
        thread.name || thread.preview || "External Codex session",
        28,
      );
      const summary = summaryFor(thread, status);
      if (managed) {
        managed.title = title;
        this.tasks.updateExternal(taskId, status, summary);
        continue;
      }
      this.tasks.createExternal({
        id: taskId,
        threadId: thread.id,
        projectId: `external-${thread.source ?? "codex"}`.slice(0, 64),
        title,
        status,
        summary,
        startedAt: timestamp(thread.createdAt),
        updatedAt: timestamp(thread.updatedAt),
        ...(thread.source ? { externalSource: thread.source } : {}),
      });
    }
    for (const task of this.tasks.all()) {
      if (task.external && !activeThreadIds.has(task.threadId))
        this.tasks.remove(task.id);
    }
  }
}
