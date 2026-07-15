import { mkdir, readFile, rename, writeFile } from "node:fs/promises";
import path from "node:path";

import type { ManagedTask } from "../tasks/task-registry.js";

export interface PersistedState {
  version: 1;
  tasks: ManagedTask[];
  display: { brightness?: number; muted?: boolean };
}

const EMPTY_STATE: PersistedState = { version: 1, tasks: [], display: {} };

export class StateStore {
  constructor(private readonly filePath: string) {}

  async load(): Promise<PersistedState> {
    try {
      const parsed = JSON.parse(await readFile(this.filePath, "utf8")) as PersistedState;
      return parsed.version === 1 && Array.isArray(parsed.tasks) ? parsed : EMPTY_STATE;
    } catch {
      return EMPTY_STATE;
    }
  }

  async save(state: PersistedState): Promise<void> {
    await mkdir(path.dirname(this.filePath), { recursive: true });
    const temporary = `${this.filePath}.${process.pid}.tmp`;
    await writeFile(temporary, `${JSON.stringify(state, null, 2)}\n`, { encoding: "utf8", mode: 0o600 });
    await rename(temporary, this.filePath);
  }
}
