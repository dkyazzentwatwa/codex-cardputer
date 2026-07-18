import os from "node:os";
import path from "node:path";
import { readFile } from "node:fs/promises";

import { parseDocument } from "yaml";
import { z } from "zod";

import { DEFAULT_PORT } from "@codexdeck/protocol";

export const bindModeSchema = z.enum(["auto_private", "loopback", "explicit"]);
const externalSourceKindSchema = z.enum([
  "cli",
  "vscode",
  "exec",
  "appServer",
  "unknown",
]);

export const bridgeSettingsSchema = z
  .object({
    version: z.literal(1).default(1),
    port: z.number().int().positive().max(65535).default(DEFAULT_PORT),
    bindMode: bindModeSchema.default("auto_private"),
    bindHost: z.string().trim().min(1).optional(),
    codexPath: z.string().trim().min(1).default("codex"),
    workflowsFile: z.string().trim().min(1),
    stateFile: z.string().trim().min(1).optional(),
    logsDirectory: z.string().trim().min(1).optional(),
    startOnLaunch: z.boolean().default(true),
    notifications: z
      .object({
        attention: z.boolean().default(true),
        deviceChanges: z.boolean().default(false),
      })
      .strict()
      .default({ attention: true, deviceChanges: false }),
    externalSessions: z
      .object({
        enabled: z.boolean().default(true),
        pollSeconds: z.number().int().min(2).max(60).default(5),
        maxSessions: z.number().int().min(1).max(20).default(8),
        cwd: z.array(z.string().trim().min(1)).max(16).default([]),
        sourceKinds: z
          .array(externalSourceKindSchema)
          .min(1)
          .max(5)
          .default(["cli", "vscode", "exec", "appServer", "unknown"]),
      })
      .strict()
      .default({
        enabled: true,
        pollSeconds: 5,
        maxSessions: 8,
        cwd: [],
        sourceKinds: ["cli", "vscode", "exec", "appServer", "unknown"],
      }),
    prettyLogs: z.boolean().default(process.env.NODE_ENV !== "production"),
  })
  .superRefine((value, context) => {
    if (value.bindMode === "explicit" && !value.bindHost)
      context.addIssue({
        code: "custom",
        path: ["bindHost"],
        message: "bindHost is required when bindMode is explicit",
      });
  })
  .strict();

export type BridgeConfig = z.infer<typeof bridgeSettingsSchema> & {
  dataDirectory: string;
  stateFile: string;
  logsDirectory: string;
};

export function defaultDataDirectory(): string {
  if (process.platform === "darwin")
    return path.join(
      os.homedir(),
      "Library",
      "Application Support",
      "CardPuter Codex Control Deck",
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
  const parsed = bridgeSettingsSchema.parse(document.toJS());
  const dataDirectory = defaultDataDirectory();
  return {
    ...parsed,
    workflowsFile: path.resolve(path.dirname(filePath), parsed.workflowsFile),
    dataDirectory,
    stateFile: parsed.stateFile
      ? path.resolve(path.dirname(filePath), parsed.stateFile)
      : path.join(dataDirectory, "state.json"),
    logsDirectory: parsed.logsDirectory
      ? path.resolve(path.dirname(filePath), parsed.logsDirectory)
      : path.join(dataDirectory, "logs"),
  };
}
