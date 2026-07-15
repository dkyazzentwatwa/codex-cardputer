import os from "node:os";

function isPrivate(address: string): boolean {
  const octets = address.split(".").map(Number);
  if (octets.length !== 4 || octets.some((value) => !Number.isInteger(value))) return false;
  const first = octets[0] ?? -1;
  const second = octets[1] ?? -1;
  return first === 10 || (first === 172 && second >= 16 && second <= 31) || (first === 192 && second === 168);
}

export function selectBindHost(interfaces = os.networkInterfaces()): string {
  const candidates: Array<{ name: string; address: string }> = [];
  for (const [name, entries] of Object.entries(interfaces)) {
    for (const entry of entries ?? []) {
      if (entry.family === "IPv4" && !entry.internal && isPrivate(entry.address)) {
        candidates.push({ name, address: entry.address });
      }
    }
  }
  candidates.sort((left, right) => left.name.localeCompare(right.name) || left.address.localeCompare(right.address));
  return candidates[0]?.address ?? "127.0.0.1";
}
