import { EventEmitter } from "node:events";
import readline from "node:readline/promises";

import type { AppServerServerRequest } from "./codex/types.js";
import type { TaskRegistry } from "./tasks/task-registry.js";

function record(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === "object" ? (value as Record<string, unknown>) : {};
}

export class DesktopInputService extends EventEmitter {
  constructor(private readonly tasks: TaskRegistry) {
    super();
  }

  handle(request: AppServerServerRequest): void {
    const params = record(request.params);
    const threadId = typeof params.threadId === "string" ? params.threadId : undefined;
    const task = threadId ? this.tasks.byThread(threadId) : undefined;
    if (!task) {
      request.reject(-32602, "Input request does not belong to a bridge-managed task");
      return;
    }
    this.tasks.transition(task.id, "waiting_input", "Answer requested on bridge desktop");
    this.emit("waiting", { taskId: task.id, params });
    if (!process.stdin.isTTY || !process.stdout.isTTY) return;
    void this.prompt(request, task.id, params);
  }

  private async prompt(request: AppServerServerRequest, taskId: string, params: Record<string, unknown>): Promise<void> {
    const terminal = readline.createInterface({ input: process.stdin, output: process.stdout });
    try {
      const questions = Array.isArray(params.questions) ? params.questions.map(record) : [];
      const answers: Record<string, { answers: string[] }> = {};
      for (const question of questions) {
        const id = typeof question.id === "string" ? question.id : `question_${Object.keys(answers).length + 1}`;
        const prompt = typeof question.question === "string" ? question.question : "Codex needs input";
        const answer = (await terminal.question(`\n[Codex input] ${prompt}\n> `)).trim();
        answers[id] = { answers: answer ? [answer] : [] };
      }
      request.respond({ answers });
      this.tasks.transition(taskId, "running", "Desktop input received");
    } catch (error) {
      request.reject(-32603, error instanceof Error ? error.message : "Desktop input failed");
      this.tasks.transition(taskId, "failed", "Desktop input failed");
    } finally {
      terminal.close();
    }
  }
}
