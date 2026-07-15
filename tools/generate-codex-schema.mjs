import { readFile, rm, mkdir } from "node:fs/promises";
import { spawnSync } from "node:child_process";
import { resolve } from "node:path";
import process from "node:process";

const root = resolve(import.meta.dirname, "..");
const output = resolve(root, ".cache", "codex-app-server-schema");
await rm(output, { recursive: true, force: true });
await mkdir(output, { recursive: true });

const generated = spawnSync("codex", ["app-server", "generate-json-schema", "--out", output], {
  encoding: "utf8",
  stdio: ["ignore", "pipe", "pipe"],
});
if (generated.status !== 0) {
  process.stderr.write(generated.stderr);
  process.exit(generated.status ?? 1);
}

const required = {
  "ClientRequest.json": [
    "initialize",
    "thread/start",
    "thread/resume",
    "turn/start",
    "turn/steer",
    "turn/interrupt",
    "skills/list",
  ],
  "ServerRequest.json": [
    "item/commandExecution/requestApproval",
    "item/fileChange/requestApproval",
    "item/tool/requestUserInput",
  ],
  "ServerNotification.json": ["turn/started", "turn/completed", "item/started", "item/completed"],
};

for (const [file, methods] of Object.entries(required)) {
  const contents = await readFile(resolve(output, file), "utf8");
  for (const method of methods) {
    if (!contents.includes(`"${method}"`)) {
      throw new Error(`Installed Codex schema is missing ${method} in ${file}`);
    }
  }
}

process.stdout.write(`Codex App Server compatibility OK: ${output}\n`);
