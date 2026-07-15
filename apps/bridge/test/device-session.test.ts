import { EventEmitter } from "node:events";

import type { WebSocket } from "ws";
import { afterEach, describe, expect, it, vi } from "vitest";

import { ApprovalService } from "../src/approvals/approval-service.js";
import { DeviceSession } from "../src/protocol/device-session.js";
import type { MessageRouter } from "../src/protocol/message-router.js";
import { TaskRegistry } from "../src/tasks/task-registry.js";

class FakeSocket extends EventEmitter {
  readonly OPEN = 1;
  readyState = 1;
  readonly send = vi.fn();
  readonly close = vi.fn();
  readonly terminate = vi.fn();
}

afterEach(() => {
  vi.useRealTimers();
});

describe("DeviceSession heartbeat", () => {
  it("pings every ten seconds and terminates after thirty seconds of silence", () => {
    vi.useFakeTimers();
    const socket = new FakeSocket();
    const tasks = new TaskRegistry();
    const approvals = new ApprovalService(tasks);
    const session = new DeviceSession(
      socket as unknown as WebSocket,
      "0.1.0",
      tasks,
      approvals,
      {} as MessageRouter,
      vi.fn(),
    );

    vi.advanceTimersByTime(10_000);
    expect(socket.send).toHaveBeenCalledWith(
      expect.stringContaining('"type":"ping"'),
    );
    vi.advanceTimersByTime(20_000);
    expect(socket.terminate).toHaveBeenCalledOnce();
    session.close();
  });
});
