import { chmod, mkdir, readFile, rename, writeFile } from "node:fs/promises";
import path from "node:path";
import process from "node:process";
import { pathToFileURL } from "node:url";

import { parseDocument } from "yaml";

import { bridgeSettingsSchema } from "../config.js";
import { registrySchema } from "../workflows/workflow-registry.js";

async function parseYaml(file: string): Promise<unknown> {
  const document = parseDocument(await readFile(file, "utf8"), {
    uniqueKeys: true,
  });
  if (document.errors.length)
    throw new Error(document.errors.map((error) => error.message).join("; "));
  return document.toJS();
}

async function atomicJson(file: string, value: unknown): Promise<void> {
  const temporary = `${file}.${process.pid}.tmp`;
  await writeFile(temporary, `${JSON.stringify(value, null, 2)}\n`, {
    encoding: "utf8",
    mode: 0o600,
  });
  await rename(temporary, file);
  await chmod(file, 0o600);
}

export async function migrateConfiguration(
  bridgeFile: string,
  workflowsFile: string,
  destination: string,
): Promise<void> {
  await mkdir(destination, { recursive: true, mode: 0o700 });
  const oldSettings = (await parseYaml(bridgeFile)) as Record<string, unknown>;
  const workflows = registrySchema.parse(await parseYaml(workflowsFile));
  const settings = bridgeSettingsSchema.parse({
    version: 1,
    port: oldSettings.port,
    bindMode: oldSettings.bindHost ? "explicit" : "auto_private",
    bindHost: oldSettings.bindHost,
    codexPath: oldSettings.codexPath,
    workflowsFile: "workflows.json",
    stateFile: "state.json",
    logsDirectory: "logs",
    startOnLaunch: true,
    notifications: { attention: true, deviceChanges: false },
    prettyLogs: false,
  });
  await atomicJson(path.join(destination, "settings.json"), settings);
  await atomicJson(path.join(destination, "workflows.json"), workflows);
}

async function main(): Promise<void> {
  const [bridgeFile, workflowsFile, destination] = process.argv.slice(2);
  if (!bridgeFile || !workflowsFile || !destination)
    throw new Error(
      "Usage: migrate-config <bridge.yaml> <workflows.yaml> <destination>",
    );
  await migrateConfiguration(bridgeFile, workflowsFile, destination);
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href)
  main().catch((error: unknown) => {
    process.stderr.write(
      `${error instanceof Error ? error.message : String(error)}\n`,
    );
    process.exitCode = 1;
  });
