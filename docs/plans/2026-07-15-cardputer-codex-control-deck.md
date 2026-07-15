# CardPuter Codex Control Deck implementation plan

This repository implements the supplied product specification with two explicit
updates: Cardputer ADV is the sole target, and Arduino CLI is the only firmware
workflow.

## Milestones

1. Scaffold a strict pnpm TypeScript monorepo and Arduino CLI ADV sketch.
2. Define and test the strict `codexdeck.v1` device protocol.
3. Add the Codex App Server JSONL adapter and installed-schema compatibility
   check for Codex CLI 0.140.0.
4. Add deterministic task, workflow, skill, approval, redaction, risk, and
   atomic persistence services.
5. Add private-LAN Fastify, WebSocket, heartbeat, mDNS, and multi-device state.
6. Add bounded firmware networking, discovery, decoding, storage, reconnect,
   and hardware-independent state logic.
7. Add the Cypher OS-inspired device UI, normalized controls, confirmations,
   Wi-Fi setup, offline recovery, and diagnostics.
8. Complete automated gates and document separately what is compile-ready,
   uploaded, and field-proven.

## Automated gates

```bash
pnpm lint
pnpm format:check
pnpm typecheck
pnpm test
pnpm build
pnpm codex:compat
arduino-cli compile --profile adv firmware/cardputer
arduino-cli compile --profile adv firmware/tests/control_deck_core
```

## Hardware gate

Upload and serial assertions use the repository helpers, both backed only by
Arduino CLI. Final field proof requires a connected Cardputer ADV, acceptance
scenarios A through H, and a 30-minute three-task soak.
