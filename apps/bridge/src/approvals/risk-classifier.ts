export interface RiskResult {
  risk: "low" | "medium" | "high";
  reason?: string;
}

const HIGH_RISK: Array<[RegExp, string]> = [
  [/\brm\s+(?:-[^\s]*r[^\s]*f|-[^\s]*f[^\s]*r)\b|\bfind\b.*\s-delete\b/i, "Recursive deletion"],
  [/\bsudo\b|\bdoas\b/i, "Privilege elevation"],
  [/\bgit\s+(?:push\s+.*--force|reset\s+--hard|clean\s+-[^\s]*f)/i, "Destructive Git operation"],
  [/\b(?:security|keychain|ssh-add|passwd)\b/i, "Credential or secret-store modification"],
  [/\b(?:diskutil|mkfs|shutdown|reboot)\b/i, "Disk or system power operation"],
  [/\b(?:pfctl|iptables|route|networksetup|systemctl)\b/i, "System network or service change"],
  [/\b(?:npm|pnpm|yarn)\s+(?:publish|login)\b/i, "Package publication or login"],
];

export function classifyRisk(input: { command?: string; cwd?: string; reason?: string }): RiskResult {
  const combined = [input.command, input.cwd, input.reason].filter(Boolean).join(" ");
  for (const [pattern, reason] of HIGH_RISK) {
    if (pattern.test(combined)) return { risk: "high", reason };
  }
  if (/\b(?:curl|wget|ssh|scp)\b/i.test(combined)) return { risk: "medium", reason: "Network access" };
  return { risk: "low" };
}
