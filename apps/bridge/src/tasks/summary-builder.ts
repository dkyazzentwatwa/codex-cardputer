import path from "node:path";

// Control-character matching is intentional for sanitizing device-bound text.
// eslint-disable-next-line no-control-regex
const ANSI = /\u001b\[[0-?]*[ -/]*[@-~]/g;
// eslint-disable-next-line no-control-regex
const CONTROL = /[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f]/g;
const SECRETS: Array<[RegExp, string]> = [
  [/\b(api[_-]?key|token|secret|password|authorization)\s*[:=]\s*[^\s,;]+/gi, "$1=[REDACTED]"],
  [/\bBearer\s+[A-Za-z0-9._~+/=-]{12,}/gi, "Bearer [REDACTED]"],
  [/\bsk-[A-Za-z0-9_-]{12,}\b/g, "[REDACTED_API_KEY]"],
];

export function truncateAtWord(value: string, maximum: number): string {
  if (value.length <= maximum) return value;
  const candidate = value.slice(0, Math.max(1, maximum - 1));
  const boundary = candidate.lastIndexOf(" ");
  const trimmed = boundary >= Math.floor(maximum * 0.6) ? candidate.slice(0, boundary) : candidate;
  return `${trimmed.trimEnd()}…`;
}

export function sanitizeSummary(value: string, maximum = 64, cwd?: string): string {
  let sanitized = value.replace(ANSI, "").replace(CONTROL, " ");
  for (const [pattern, replacement] of SECRETS) sanitized = sanitized.replace(pattern, replacement);
  if (cwd) sanitized = sanitized.replaceAll(cwd, path.basename(cwd));
  sanitized = sanitized.replace(/(?:\/[\w.@ -]+){3,}/g, (match) => `…/${path.basename(match)}`);
  sanitized = sanitized.replace(/\s+/g, " ").trim();
  return truncateAtWord(sanitized || "Working on task", maximum);
}

export function commandCategory(command: string): string {
  const executable = command.trim().split(/\s+/)[0] ?? "command";
  const categories: Record<string, string> = {
    git: "Running Git command",
    pnpm: "Running project checks",
    npm: "Running project checks",
    yarn: "Running project checks",
    "arduino-cli": "Building Cardputer firmware",
    rg: "Searching project files",
    sed: "Reading project files",
  };
  return categories[executable] ?? `Running ${path.basename(executable)}`;
}
