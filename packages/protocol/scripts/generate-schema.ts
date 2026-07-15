import { mkdir, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { z } from "zod";

import { protocolBundleSchema } from "../src/schemas.js";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const outputDirectory = resolve(root, "schema");
await mkdir(outputDirectory, { recursive: true });
await writeFile(
  resolve(outputDirectory, "control-deck-v1.schema.json"),
  `${JSON.stringify(z.toJSONSchema(protocolBundleSchema), null, 2)}\n`,
  "utf8",
);
