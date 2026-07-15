# codexdeck.v1 protocol

## Transport

- WebSocket endpoint: `/device`
- Health endpoint: `GET /healthz`
- Default port: `8765`
- mDNS service: `_codexdeck._tcp.local`
- JSON text frames only
- Maximum frame: 8,192 bytes in either direction
- Heartbeat: 10 seconds
- Stale timeout: 30 seconds

The device starts with `hello` containing protocol `codexdeck.v1`, a stable
device ID, firmware version, and enumerated capabilities. The bridge answers
with `welcome`, then sends full `task.snapshot` and `macro.snapshot` messages.
Open approvals follow the snapshots.

## Server to device

- `welcome`
- `task.snapshot`, `task.upsert`, `task.remove`
- `approval.open`, `approval.resolved`
- `macro.snapshot`
- `toast`, `error`
- `ping`

## Device to server

- `hello`, `pong`
- `task.select`, `snapshot.request`
- `task.stop.request`
- `task.followup.submit`
- `workflow.launch.request`, `skill.launch.request`
- `approval.respond`

There is intentionally no generic command or shell message.

## Limits and validation

All external frames are parsed by strict Zod schemas. Unknown fields and
message types fail validation. Snapshots contain at most 20 tasks and 20 global
macros. Follow-ups are nonempty and at most 240 UTF-8 bytes. Device approval
decisions are limited to `accept`, `decline`, and `cancel`.

Task display limits are 28 characters for title, 64 for summary, and 160 for
detail. Display text is stripped of ANSI and control characters, redacted for
common secrets, shortened for deep paths, whitespace-collapsed, and truncated
at a word boundary when possible.

The source schemas live in `packages/protocol/src/schemas.ts`. The committed
JSON Schema is generated from those definitions:

```bash
pnpm --filter @codexdeck/protocol generate:schema
```

## Idempotency

Every mutation includes a `requestId`. The bridge caches the last 100 results
for each device for 10 minutes. A duplicate gets the original bounded result
and never launches a second thread or repeats an approval response.

## App Server compatibility

The bridge was initially tested with Codex CLI 0.140.0. Development generates
the installed App Server schema into ignored `.cache` storage and compiles a
narrow adapter against the known methods:

```bash
pnpm codex:compat
```

The generated Codex protocol is intentionally not committed.
