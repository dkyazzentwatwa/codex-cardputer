import type { NetworkInterfaceInfo } from "node:os";

import { describe, expect, it } from "vitest";

import { selectBindHost } from "../src/network/private-address.js";
import { RequestCache } from "../src/protocol/request-cache.js";

function interfaceInfo(
  address: string,
  internal = false,
): NetworkInterfaceInfo {
  return {
    address,
    netmask: "255.255.255.0",
    family: "IPv4",
    mac: "00:00:00:00:00:00",
    internal,
    cidr: `${address}/24`,
  };
}

describe("network helpers", () => {
  it("selects a deterministic private IPv4 address", () => {
    expect(
      selectBindHost({
        z_wifi: [interfaceInfo("192.168.1.8")],
        a_usb: [interfaceInfo("10.0.0.2")],
      }),
    ).toBe("10.0.0.2");
    expect(selectBindHost({ lo0: [interfaceInfo("127.0.0.1", true)] })).toBe(
      "127.0.0.1",
    );
  });

  it("replays idempotent request results", () => {
    const cache = new RequestCache();
    cache.set(
      "req-1",
      [
        {
          type: "toast",
          level: "success",
          message: "done",
          requestId: "req-1",
        },
      ],
      100,
    );
    expect(cache.get("req-1", 101)?.[0]).toMatchObject({
      type: "toast",
      message: "done",
    });
  });
});
