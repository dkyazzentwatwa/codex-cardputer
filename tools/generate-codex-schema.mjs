import { readFile, rm, mkdir, writeFile } from "node:fs/promises";
import { spawnSync } from "node:child_process";
import { resolve } from "node:path";
import process from "node:process";

const root = resolve(import.meta.dirname, "..");
const output = resolve(root, ".cache", "codex-app-server-schema");
const typeOutput = resolve(root, ".cache", "codex-app-server-types");
await rm(output, { recursive: true, force: true });
await rm(typeOutput, { recursive: true, force: true });
await mkdir(output, { recursive: true });

const generated = spawnSync(
  "codex",
  ["app-server", "generate-json-schema", "--out", output],
  {
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  },
);
if (generated.status !== 0) {
  process.stderr.write(generated.stderr);
  process.exit(generated.status ?? 1);
}

const generatedTypes = spawnSync(
  "codex",
  ["app-server", "generate-ts", "--out", typeOutput],
  {
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  },
);
if (generatedTypes.status !== 0) {
  process.stderr.write(generatedTypes.stderr);
  process.exit(generatedTypes.status ?? 1);
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
  "ServerNotification.json": [
    "turn/started",
    "turn/completed",
    "item/started",
    "item/completed",
  ],
};

for (const [file, methods] of Object.entries(required)) {
  const contents = await readFile(resolve(output, file), "utf8");
  for (const method of methods) {
    if (!contents.includes(`"${method}"`)) {
      throw new Error(`Installed Codex schema is missing ${method} in ${file}`);
    }
  }
}

const compatibilityFile = resolve(root, ".cache", "codex-adapter-compat.ts");
await writeFile(
  compatibilityFile,
  `import type { ThreadStartParams } from "./codex-app-server-types/v2/ThreadStartParams";
import type { ThreadStartResponse } from "./codex-app-server-types/v2/ThreadStartResponse";
import type { TurnStartResponse } from "./codex-app-server-types/v2/TurnStartResponse";
import type { UserInput } from "./codex-app-server-types/v2/UserInput";
import type { SkillMetadata as GeneratedSkill } from "./codex-app-server-types/v2/SkillMetadata";
import type { AppServerThread, AppServerTurn, AppServerUserInput, SkillMetadata } from "../apps/bridge/src/codex/types";

type Assert<T extends true> = T;
type NarrowThreadStart = {
  cwd: string;
  model?: string;
  approvalPolicy?: "untrusted" | "on-request" | "never";
  sandbox?: "read-only" | "workspace-write" | "danger-full-access";
};
type ThreadParamsCompatible = Assert<NarrowThreadStart extends ThreadStartParams ? true : false>;
type UserInputCompatible = Assert<AppServerUserInput extends UserInput ? true : false>;
type SkillCompatible = Assert<GeneratedSkill extends SkillMetadata ? true : false>;
type ThreadCompatible = Assert<ThreadStartResponse["thread"] extends AppServerThread ? true : false>;
type TurnCompatible = Assert<TurnStartResponse["turn"] extends AppServerTurn ? true : false>;
`,
);

const checked = spawnSync(
  "pnpm",
  [
    "exec",
    "tsc",
    "--noEmit",
    "--strict",
    "--skipLibCheck",
    "--module",
    "esnext",
    "--moduleResolution",
    "bundler",
    compatibilityFile,
  ],
  { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] },
);
if (checked.status !== 0) {
  process.stdout.write(checked.stdout);
  process.stderr.write(checked.stderr);
  process.exit(checked.status ?? 1);
}

process.stdout.write(
  `Codex App Server schema and narrow adapter compatibility OK: ${output}\n`,
);
