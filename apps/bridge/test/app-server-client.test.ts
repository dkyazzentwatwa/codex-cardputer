import { EventEmitter } from "node:events";
import { PassThrough } from "node:stream";

import { describe, expect, it } from "vitest";

import { AppServerClient } from "../src/codex/app-server-client.js";

class FakeProcess extends EventEmitter {
  readonly stdin = new PassThrough();
  readonly stdout = new PassThrough();
  readonly stderr = new PassThrough();

  override once(event: "exit", listener: (code: number | null, signal: NodeJS.Signals | null) => void): this {
    return super.once(event, listener);
  }

  kill(): boolean {
    return true;
  }
}

describe("AppServerClient", () => {
  it("initializes once and uses thread/turn methods", async () => {
    const process = new FakeProcess();
    let buffer = "";
    process.stdin.on("data", (chunk) => {
      buffer += chunk.toString();
      const lines = buffer.split("\n");
      buffer = lines.pop() ?? "";
      for (const line of lines) {
        if (!line) continue;
        const message = JSON.parse(line) as { id?: number; method: string };
        if (message.id === undefined) continue;
        const result =
          message.method === "initialize"
            ? { userAgent: "test" }
            : message.method === "thread/start"
              ? { thread: { id: "thread-1" } }
              : { turn: { id: "turn-1" } };
        process.stdout.write(`${JSON.stringify({ id: message.id, result })}\n`);
      }
    });
    const client = new AppServerClient({ clientVersion: "0.1.0", spawnProcess: () => process });
    await client.start();
    await expect(client.startThread({ cwd: "/tmp", approvalPolicy: "on-request" })).resolves.toEqual({
      id: "thread-1",
    });
    await expect(
      client.startTurn("thread-1", [{ type: "text", text: "hello", text_elements: [] }]),
    ).resolves.toEqual({ id: "turn-1" });
    expect(client.ready).toBe(true);
    client.stop();
  });
});
