import { REQUEST_CACHE_SIZE, REQUEST_CACHE_TTL_MS } from "@codexdeck/protocol";
import type { ServerMessage } from "@codexdeck/protocol";

interface Entry {
  createdAt: number;
  messages: ServerMessage[];
}

export class RequestCache {
  private readonly entries = new Map<string, Entry>();

  get(requestId: string, now = Date.now()): ServerMessage[] | undefined {
    this.prune(now);
    const entry = this.entries.get(requestId);
    return entry ? structuredClone(entry.messages) : undefined;
  }

  set(requestId: string, messages: ServerMessage[], now = Date.now()): void {
    this.prune(now);
    this.entries.delete(requestId);
    this.entries.set(requestId, { createdAt: now, messages: structuredClone(messages) });
    while (this.entries.size > REQUEST_CACHE_SIZE) {
      const oldest = this.entries.keys().next().value as string | undefined;
      if (!oldest) break;
      this.entries.delete(oldest);
    }
  }

  private prune(now: number): void {
    for (const [requestId, entry] of this.entries) {
      if (now - entry.createdAt > REQUEST_CACHE_TTL_MS) this.entries.delete(requestId);
    }
  }
}
