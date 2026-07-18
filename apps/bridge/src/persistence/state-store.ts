import { chmod, mkdir, readFile, rename, writeFile } from "node:fs/promises";
import path from "node:path";

import { taskStatusSchema, taskSummarySchema } from "@codexdeck/protocol";
import { z } from "zod";

import type { ManagedTask } from "../tasks/task-registry.js";

export interface PersistedState {
  version: 1;
  tasks: ManagedTask[];
  display: { brightness?: number; muted?: boolean };
}

const EMPTY_STATE: PersistedState = { version: 1, tasks: [], display: {} };
const stateSchema = z
  .object({
    version: z.literal(1),
    tasks: z.array(
      taskSummarySchema.extend({
        workflowId: z.string().trim().min(1).max(64).optional(),
        previousActiveStatus: taskStatusSchema.optional(),
      }),
    ),
    display: z
      .object({
        brightness: z.number().int().min(0).max(255).optional(),
        muted: z.boolean().optional(),
      })
      .strict(),
  })
  .strict();

export class StateStore {
  constructor(private readonly filePath: string) {}

  async load(): Promise<PersistedState> {
    try {
      return stateSchema.parse(
        JSON.parse(await readFile(this.filePath, "utf8")),
      ) as PersistedState;
    } catch {
      return EMPTY_STATE;
    }
  }

  async save(state: PersistedState): Promise<void> {
    await mkdir(path.dirname(this.filePath), { recursive: true });
    const temporary = `${this.filePath}.${process.pid}.tmp`;
    await writeFile(temporary, `${JSON.stringify(state, null, 2)}\n`, {
      encoding: "utf8",
      mode: 0o600,
    });
    await rename(temporary, this.filePath);
    await chmod(this.filePath, 0o600);
  }
}
