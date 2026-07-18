import { describe, expect, it } from "vitest";

import {
  MAX_FRAME_BYTES,
  PROTOCOL_VERSION,
  ProtocolError,
  deviceMessageSchema,
  parseDeviceFrame,
  serverMessageSchema,
} from "../src/index.js";

const hello = {
  type: "hello",
  protocol: PROTOCOL_VERSION,
  deviceId: "cardputer-a1b2c3",
  deviceName: "CardPuter",
  firmwareVersion: "0.1.0",
  capabilities: ["keyboard", "display", "hold-confirm"],
} as const;

describe("codexdeck.v1 schemas", () => {
  it("accepts the device handshake", () => {
    expect(deviceMessageSchema.parse(hello)).toEqual(hello);
  });

  it("accepts a bounded task snapshot", () => {
    const message = {
      type: "task.snapshot",
      tasks: [
        {
          id: "task-1",
          threadId: "thread-1",
          projectId: "project-1",
          title: "Control Deck",
          status: "running",
          summary: "Running protocol tests",
          detail: "Readable result ".repeat(40).trim(),
          startedAt: "2026-07-15T19:00:00.000Z",
          updatedAt: "2026-07-15T19:00:01.000Z",
          elapsedSeconds: 1,
          requiresAttention: false,
          macros: [],
        },
      ],
    };
    expect(serverMessageSchema.parse(message)).toEqual(message);
  });

  it("accepts only device-safe approval decisions", () => {
    expect(
      deviceMessageSchema.parse({
        type: "approval.respond",
        requestId: "req-1",
        approvalId: "approval-1",
        decision: "accept",
      }),
    ).toBeTruthy();
    expect(
      deviceMessageSchema.safeParse({
        type: "approval.respond",
        requestId: "req-1",
        approvalId: "approval-1",
        decision: "acceptForSession",
      }).success,
    ).toBe(false);
  });

  it("rejects unknown fields", () => {
    expect(
      deviceMessageSchema.safeParse({ ...hello, token: "secret" }).success,
    ).toBe(false);
  });

  it("rejects protocol mismatches", () => {
    expect(
      deviceMessageSchema.safeParse({ ...hello, protocol: "codexdeck.v2" })
        .success,
    ).toBe(false);
  });

  it("enforces the maximum frame size", () => {
    expect(() =>
      parseDeviceFrame(Buffer.alloc(MAX_FRAME_BYTES + 1, "x")),
    ).toThrowError(ProtocolError);
  });

  it("rejects malformed JSON without exposing parser details", () => {
    expect(() => parseDeviceFrame("{")).toThrowError(
      expect.objectContaining({ code: "MALFORMED_JSON" }),
    );
  });

  it("enforces the 240-byte follow-up limit", () => {
    const message = {
      type: "task.followup.submit",
      requestId: "req-2",
      taskId: "task-1",
      prompt: "é".repeat(121),
    };
    expect(deviceMessageSchema.safeParse(message).success).toBe(false);
  });

  it("accepts only the strict clear-finished request shape", () => {
    expect(
      deviceMessageSchema.safeParse({
        type: "tasks.clear.request",
        requestId: "clear-1",
      }).success,
    ).toBe(true);
    expect(
      deviceMessageSchema.safeParse({
        type: "tasks.clear.request",
        requestId: "clear-1",
        includeActive: true,
      }).success,
    ).toBe(false);
  });

  it("accepts bounded Codex usage windows", () => {
    expect(
      serverMessageSchema.parse({
        type: "usage.update",
        available: true,
        limitName: "Codex",
        primaryRemainingPercent: 72,
        primaryWindowMinutes: 300,
        secondaryRemainingPercent: 41,
        secondaryWindowMinutes: 10080,
      }),
    ).toMatchObject({ primaryRemainingPercent: 72 });
    expect(
      serverMessageSchema.safeParse({
        type: "usage.update",
        available: true,
        primaryRemainingPercent: 101,
      }).success,
    ).toBe(false);
  });
});
