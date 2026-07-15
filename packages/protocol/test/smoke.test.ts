import { describe, expect, it } from "vitest";

import { PROTOCOL_VERSION } from "../src/index.js";

describe("protocol workspace", () => {
  it("exports the v1 marker", () => {
    expect(PROTOCOL_VERSION).toBe("codexdeck.v1");
  });
});
