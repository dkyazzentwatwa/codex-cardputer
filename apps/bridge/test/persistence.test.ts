import { mkdtemp, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

import { describe, expect, it } from "vitest";

import { StateStore } from "../src/persistence/state-store.js";

describe("StateStore", () => {
  it("writes and reloads state atomically", async () => {
    const root = await mkdtemp(path.join(os.tmpdir(), "codexdeck-state-"));
    const file = path.join(root, "state.json");
    const store = new StateStore(file);
    await store.save({ version: 1, tasks: [], display: { brightness: 180 } });
    await expect(store.load()).resolves.toEqual({
      version: 1,
      tasks: [],
      display: { brightness: 180 },
    });
  });

  it("recovers from corrupt state", async () => {
    const root = await mkdtemp(path.join(os.tmpdir(), "codexdeck-state-"));
    const file = path.join(root, "state.json");
    await writeFile(file, "not-json");
    await expect(new StateStore(file).load()).resolves.toEqual({
      version: 1,
      tasks: [],
      display: {},
    });
  });

  it("rejects structurally invalid state without restoring it", async () => {
    const root = await mkdtemp(path.join(os.tmpdir(), "codexdeck-state-"));
    const file = path.join(root, "state.json");
    await writeFile(
      file,
      JSON.stringify({ version: 1, tasks: [{ status: "root" }], display: {} }),
    );
    await expect(new StateStore(file).load()).resolves.toEqual({
      version: 1,
      tasks: [],
      display: {},
    });
  });
});
