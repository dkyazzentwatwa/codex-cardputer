import { appendFile, mkdir, rename, stat } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import process from "node:process";
import { EventEmitter } from "node:events";

export type LogLevel = "info" | "warn" | "error";

export interface LogEntry {
  timestamp: string;
  level: LogLevel;
  component: string;
  event: string;
  fields: Record<string, unknown>;
}

const SENSITIVE_KEY = /token|password|secret|authorization|credential/i;

function redact(value: unknown, key = ""): unknown {
  if (SENSITIVE_KEY.test(key)) return "[REDACTED]";
  if (typeof value === "string")
    return value.replaceAll(os.homedir(), "~").slice(0, 2_000);
  if (Array.isArray(value)) return value.map((item) => redact(item));
  if (value && typeof value === "object")
    return Object.fromEntries(
      Object.entries(value as Record<string, unknown>).map(
        ([childKey, child]) => [childKey, redact(child, childKey)],
      ),
    );
  return value;
}

export class StructuredLogger extends EventEmitter {
  private readonly recentEntries: LogEntry[] = [];
  private writeQueue: Promise<void> = Promise.resolve();
  private lastError: string | undefined;

  constructor(
    private readonly directory: string,
    private readonly pretty = false,
    private readonly maxBytes = 1_000_000,
    private readonly retainedFiles = 5,
  ) {
    super();
  }

  log(
    level: LogLevel,
    component: string,
    event: string,
    fields: Record<string, unknown> = {},
  ): void {
    const safeFields = redact(fields) as Record<string, unknown>;
    const entry: LogEntry = {
      timestamp: new Date().toISOString(),
      level,
      component,
      event,
      fields: safeFields,
    };
    if (level === "error")
      this.lastError =
        typeof safeFields.error === "string" ? safeFields.error : event;
    this.recentEntries.push(entry);
    if (this.recentEntries.length > 200) this.recentEntries.shift();
    process.stdout.write(
      this.pretty
        ? `[${entry.timestamp}] ${level.toUpperCase()} ${component}.${event} ${JSON.stringify(safeFields)}\n`
        : `${JSON.stringify({
            timestamp: entry.timestamp,
            level,
            component,
            event,
            ...safeFields,
          })}\n`,
    );
    this.emit("entry", entry);
    this.writeQueue = this.writeQueue
      .then(() => this.append(entry))
      .catch((error: unknown) => {
        process.stderr.write(`CodexDeck log write failed: ${String(error)}\n`);
      });
  }

  recent(limit = 200): LogEntry[] {
    return this.recentEntries.slice(-Math.max(1, Math.min(limit, 200)));
  }

  latestError(): string | undefined {
    return this.lastError;
  }

  async flush(): Promise<void> {
    await this.writeQueue;
  }

  private async append(entry: LogEntry): Promise<void> {
    await mkdir(this.directory, { recursive: true, mode: 0o700 });
    const file = path.join(this.directory, "bridge.ndjson");
    const line = `${JSON.stringify(entry)}\n`;
    let size = 0;
    try {
      size = (await stat(file)).size;
    } catch {
      // The first entry creates the file.
    }
    if (size + Buffer.byteLength(line) > this.maxBytes) await this.rotate(file);
    await appendFile(file, line, { encoding: "utf8", mode: 0o600 });
  }

  private async rotate(file: string): Promise<void> {
    for (let index = this.retainedFiles - 1; index >= 1; index -= 1) {
      const source = index === 1 ? file : `${file}.${index - 1}`;
      const destination = `${file}.${index}`;
      try {
        await rename(source, destination);
      } catch {
        // Missing generations are expected.
      }
    }
  }
}
