# Architecture

## Trust boundary

The Cardputer is a bounded display and input client. The desktop bridge owns
Codex App Server, task state, workflow validation, skill discovery, approvals,
persistence, redaction, and LAN policy. Codex App Server remains a local stdio
child process and is never placed on the LAN.

```text
Cardputer ADV -> ws://private-host:8765/device -> TypeScript bridge
                                                    |
                                                    v
                                     codex app-server --listen stdio://
```

There is no arbitrary command message, shell interpolation, raw output stream,
cloud relay, or device-side Codex credential.

## Bridge components

- `AppServerClient` initializes Codex, correlates JSONL requests, forwards
  notifications and server requests, rejects pending calls on exit, restarts
  with capped exponential backoff, and resumes bridge-managed threads.
- `EventNormalizer` maps App Server notifications into the stable task state
  machine without another model call.
- `TaskRegistry` enforces transitions, ordering, bounded summaries, attention
  state, and per-task context macros.
- `WorkflowRegistry` strictly validates YAML and existing absolute project
  roots. Values are data and are never evaluated by a shell.
- `WorkflowRunner` starts or resumes only configured projects and supplies
  skills as structured App Server inputs.
- `ApprovalService` accepts only approvals belonging to managed threads,
  redacts their display payload, classifies UI risk, and exposes only `accept`,
  `decline`, and `cancel` to devices.
- `ControlDeckServer` provides `/healthz`, `/device`, private-interface binding,
  mDNS, heartbeats, consistent broadcasts, and per-device idempotency.
- `StateStore` atomically persists compact managed-task mappings. It does not
  save prompts, command output, credentials, or resolved approval payloads.

## Recovery

When App Server exits, pending RPC calls reject and active tasks become stale.
The adapter restarts after 0.5, 1, 2, 4, 8, 16, then 30 seconds. Known
nonterminal threads are resumed and return to their previous active state only
after App Server confirms the thread.

When the bridge restarts, persisted nonterminal tasks load as stale and follow
the same resume path. Every device receives a full task and macro snapshot after
handshake. Multiple devices receive the same task and approval broadcasts.

## Firmware

The Arduino sketch separates hardware-independent logic into the local
`ControlDeckCore` library. It owns fixed-capacity task and macro records,
sorting, selection retention, stale marking, reconnect delay, and approval
confirmation. The same library is compiled by the production and serial test
sketches.

Production firmware contains:

- ESP32 Preferences-backed Wi-Fi and bridge hints;
- periodic `_codexdeck._tcp` discovery and saved-host fallback;
- WebSocket reconnect with capped exponential delay and jitter;
- an 8 KB frame limit and a fixed 32 KB JSON decode arena;
- normalized Cardputer keyboard input and a keymap diagnostic screen;
- dashboard, task, macro, approval, follow-up, Wi-Fi, offline, diagnostics,
  and stop-confirmation screens;
- mutation blocking whenever the bridge handshake is not active.

The display uses a 240 by 135 dark high-contrast layout with cyan selection,
compact headers and footers, four task rows, text status labels, and explicit
offline and diagnostic states.
