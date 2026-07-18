import { EventEmitter } from "node:events";
import readline from "node:readline/promises";

import type { AppServerServerRequest } from "./codex/types.js";
import type { TaskRegistry } from "./tasks/task-registry.js";
import { z } from "zod";

const inputAnswersSchema = z.record(
  z.string().trim().min(1).max(128),
  z.object({ answers: z.array(z.string().max(8_000)).max(10) }).strict(),
);

export type DesktopInputAnswers = z.infer<typeof inputAnswersSchema>;

export interface PendingDesktopInput {
  id: string;
  taskId: string;
  createdAt: string;
  params: Record<string, unknown>;
}

function record(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === "object"
    ? (value as Record<string, unknown>)
    : {};
}

export class DesktopInputService extends EventEmitter {
  private promptQueue: Promise<void> = Promise.resolve();
  private readonly pending = new Map<
    string,
    { request: AppServerServerRequest; input: PendingDesktopInput }
  >();

  constructor(private readonly tasks: TaskRegistry) {
    super();
  }

  handle(request: AppServerServerRequest): void {
    const params = record(request.params);
    const threadId =
      typeof params.threadId === "string" ? params.threadId : undefined;
    const task = threadId ? this.tasks.byThread(threadId) : undefined;
    if (!task) {
      request.reject(
        -32602,
        "Input request does not belong to a bridge-managed task",
      );
      return;
    }
    this.tasks.transition(
      task.id,
      "waiting_input",
      "Answer requested on bridge desktop",
    );
    const id = String(request.id);
    const input: PendingDesktopInput = {
      id,
      taskId: task.id,
      createdAt: new Date().toISOString(),
      params,
    };
    this.pending.set(id, { request, input });
    this.emit("waiting", input);
    if (!process.stdin.isTTY || !process.stdout.isTTY) return;
    this.promptQueue = this.promptQueue
      .catch(() => undefined)
      .then(() => this.prompt(id, params));
  }

  list(): PendingDesktopInput[] {
    return [...this.pending.values()].map(({ input }) => input);
  }

  respond(id: string, answers: unknown): PendingDesktopInput {
    const pending = this.pending.get(id);
    if (!pending) throw new Error(`Input request not found: ${id}`);
    const parsed = inputAnswersSchema.parse(answers);
    pending.request.respond({ answers: parsed });
    this.pending.delete(id);
    this.tasks.transition(
      pending.input.taskId,
      "running",
      "Desktop input received",
    );
    this.emit("resolved", { id, taskId: pending.input.taskId });
    return pending.input;
  }

  private async prompt(
    id: string,
    params: Record<string, unknown>,
  ): Promise<void> {
    const terminal = readline.createInterface({
      input: process.stdin,
      output: process.stdout,
    });
    try {
      const questions = Array.isArray(params.questions)
        ? params.questions.map(record)
        : [];
      const answers: Record<string, { answers: string[] }> = {};
      for (const question of questions) {
        const id =
          typeof question.id === "string"
            ? question.id
            : `question_${Object.keys(answers).length + 1}`;
        const prompt =
          typeof question.question === "string"
            ? question.question
            : "Codex needs input";
        const answer = (
          await terminal.question(`\n[Codex input] ${prompt}\n> `)
        ).trim();
        answers[id] = { answers: answer ? [answer] : [] };
      }
      this.respond(id, answers);
    } catch (error) {
      const pending = this.pending.get(id);
      pending?.request.reject(
        -32603,
        error instanceof Error ? error.message : "Desktop input failed",
      );
      if (pending) {
        this.pending.delete(id);
        this.tasks.transition(
          pending.input.taskId,
          "failed",
          "Desktop input failed",
        );
      }
    } finally {
      terminal.close();
    }
  }
}
