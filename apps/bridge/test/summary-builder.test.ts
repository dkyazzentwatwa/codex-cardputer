import { describe, expect, it } from "vitest";

import {
  commandCategory,
  sanitizeSummary,
} from "../src/tasks/summary-builder.js";

describe("summary builder", () => {
  it("strips ANSI, secrets, controls, and long paths", () => {
    const summary = sanitizeSummary(
      "\u001b[31mRun\u001b[0m token=abc123 /Users/person/project/src/private/file.ts\u0007",
      80,
    );
    expect(summary).not.toContain("\u001b");
    expect(summary).not.toContain("abc123");
    expect(summary).toContain("…/file.ts");
  });

  it("truncates at a readable boundary", () => {
    expect(sanitizeSummary("one two three four five", 14)).toBe(
      "one two three…",
    );
  });

  it("categorizes commands without exposing output", () => {
    expect(commandCategory("pnpm test --run")).toBe("Running project checks");
  });
});
