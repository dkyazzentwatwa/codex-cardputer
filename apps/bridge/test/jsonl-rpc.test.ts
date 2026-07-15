import { PassThrough } from "node:stream";

import { describe, expect, it } from "vitest";

import { JsonlRpcTransport, RpcRemoteError } from "../src/codex/jsonl-rpc.js";
import type { AppServerServerRequest } from "../src/codex/types.js";

describe("JsonlRpcTransport", () => {
  it("buffers partial lines and correlates responses", async () => {
    const input = new PassThrough();
    const output = new PassThrough();
    const written: string[] = [];
    input.on("data", (chunk) => written.push(chunk.toString()));
    const transport = new JsonlRpcTransport(input, output);
    const pending = transport.request<{ ok: boolean }>("thread/start", {});
    expect(written.join("")).toContain('"method":"thread/start"');
    output.write('{"id":1,"res');
    output.write('ult":{"ok":true}}\n');
    await expect(pending).resolves.toEqual({ ok: true });
  });

  it("isolates malformed lines and continues", async () => {
    const input = new PassThrough();
    const output = new PassThrough();
    const transport = new JsonlRpcTransport(input, output);
    const malformed: string[] = [];
    transport.on("malformed", (line: string) => malformed.push(line));
    const pending = transport.request("turn/start", {});
    output.write("not-json\n");
    output.write('{"id":1,"result":{"ok":true}}\n');
    await expect(pending).resolves.toEqual({ ok: true });
    expect(malformed).toEqual(["not-json"]);
  });

  it("returns server-initiated request responses", async () => {
    const input = new PassThrough();
    const output = new PassThrough();
    const written: string[] = [];
    input.on("data", (chunk) => written.push(chunk.toString()));
    const transport = new JsonlRpcTransport(input, output);
    transport.on("request", (request: AppServerServerRequest) => request.respond({ decision: "accept" }));
    output.write('{"id":"approval-1","method":"item/fileChange/requestApproval","params":{}}\n');
    await new Promise((resolve) => setImmediate(resolve));
    expect(written.join("")).toContain('"decision":"accept"');
  });

  it("rejects remote errors", async () => {
    const input = new PassThrough();
    const output = new PassThrough();
    const transport = new JsonlRpcTransport(input, output);
    const pending = transport.request("thread/resume", {});
    output.write('{"id":1,"error":{"code":404,"message":"missing"}}\n');
    await expect(pending).rejects.toBeInstanceOf(RpcRemoteError);
  });
});
