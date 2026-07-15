import { MAX_FRAME_BYTES } from "./constants.js";
import type { DeviceMessage, ServerMessage } from "./messages.js";
import { deviceMessageSchema, serverMessageSchema } from "./schemas.js";

export class ProtocolError extends Error {
  constructor(
    readonly code: "FRAME_TOO_LARGE" | "MALFORMED_JSON" | "INVALID_MESSAGE",
    message: string,
  ) {
    super(message);
    this.name = "ProtocolError";
  }
}

function decodeJson(frame: string | Buffer): unknown {
  const bytes = Buffer.byteLength(frame);
  if (bytes > MAX_FRAME_BYTES) {
    throw new ProtocolError("FRAME_TOO_LARGE", `Frame exceeds ${MAX_FRAME_BYTES} bytes`);
  }
  try {
    return JSON.parse(frame.toString()) as unknown;
  } catch {
    throw new ProtocolError("MALFORMED_JSON", "Frame is not valid JSON");
  }
}

export function parseDeviceFrame(frame: string | Buffer): DeviceMessage {
  const result = deviceMessageSchema.safeParse(decodeJson(frame));
  if (!result.success) {
    throw new ProtocolError("INVALID_MESSAGE", result.error.issues[0]?.message ?? "Invalid device message");
  }
  return result.data;
}

export function parseServerFrame(frame: string | Buffer): ServerMessage {
  const result = serverMessageSchema.safeParse(decodeJson(frame));
  if (!result.success) {
    throw new ProtocolError("INVALID_MESSAGE", result.error.issues[0]?.message ?? "Invalid server message");
  }
  return result.data;
}

export function encodeServerMessage(message: ServerMessage): string {
  return JSON.stringify(serverMessageSchema.parse(message));
}
