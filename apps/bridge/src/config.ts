import os from "node:os";
import path from "node:path";
import { readFile } from "node:fs/promises";

import { parseDocument } from "yaml";
import { z } from "zod";

import { DEFAULT_PORT } from "@codexdeck/protocol";

const configSchema = z
  .object({
    port: z.number().int().positive().max(65535).default(DEFAULT_PORT),
    bindHost: z.string().trim().min(1).optional(),
    codexPath: z.string().trim().min(1).default("codex"),
    workflowsFile: z.string().trim().min(1),
    stateFile: z.string().trim().min(1).optional(),
    prettyLogs: z.boolean().default(process.env.NODE_ENV !== "production"),
  })
  .strict();

export type BridgeConfig = z.infer<typeof configSchema> & {
  dataDirectory: string;
  stateFile: string;
};

export function defaultDataDirectory(): string {
  if (process.platform === "darwin")
    return path.join(
      os.homedir(),
      "Library",
      "Application Support",
      "cardputer-codex-control",
    );
  if (process.platform === "win32")
    return path.join(
      process.env.APPDATA ?? path.join(os.homedir(), "AppData", "Roaming"),
      "cardputer-codex-control",
    );
  return path.join(
    process.env.XDG_STATE_HOME ?? path.join(os.homedir(), ".local", "state"),
    "cardputer-codex-control",
  );
}

export async function loadConfig(filePath: string): Promise<BridgeConfig> {
  const document = parseDocument(await readFile(filePath, "utf8"), {
    uniqueKeys: true,
  });
  if (document.errors.length > 0)
    throw new Error(document.errors.map((error) => error.message).join("; "));
  const parsed = configSchema.parse(document.toJS());
  const dataDirectory = defaultDataDirectory();
  return {
    ...parsed,
    workflowsFile: path.resolve(path.dirname(filePath), parsed.workflowsFile),
    dataDirectory,
    stateFile: parsed.stateFile
      ? path.resolve(path.dirname(filePath), parsed.stateFile)
      : path.join(dataDirectory, "state.json"),
  };
}
