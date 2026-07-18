import { mkdtemp, readFile, stat } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

import { afterEach, describe, expect, it, vi } from "vitest";

import { StructuredLogger } from "../src/logging/structured-logger.js";

afterEach(() => vi.restoreAllMocks());

describe("StructuredLogger", () => {
  it("redacts sensitive fields, shortens home paths, and rotates bounded files", async () => {
    vi.spyOn(process.stdout, "write").mockReturnValue(true);
    const directory = await mkdtemp(path.join(os.tmpdir(), "codexdeck-log-"));
    const logger = new StructuredLogger(directory, false, 250, 3);
    logger.log("info", "test", "first", {
      token: "secret-value",
      project: path.join(os.homedir(), "Documents", "project"),
    });
    logger.log("warn", "test", "second", { message: "x".repeat(180) });
    logger.log("error", "test", "third", { password: "do-not-log" });
    await logger.flush();

    const current = await readFile(
      path.join(directory, "bridge.ndjson"),
      "utf8",
    );
    const rotated = await readFile(
      path.join(directory, "bridge.ndjson.1"),
      "utf8",
    );
    const oldest = await readFile(
      path.join(directory, "bridge.ndjson.2"),
      "utf8",
    );
    const combined = `${current}${rotated}${oldest}`;
    expect(combined).toContain("[REDACTED]");
    expect(combined).toContain("~/Documents/project");
    expect(combined).not.toContain("secret-value");
    expect(combined).not.toContain(os.homedir());
    expect(logger.latestError()).toBe("third");
    expect(
      (await stat(path.join(directory, "bridge.ndjson"))).mode & 0o777,
    ).toBe(0o600);
  });
});
