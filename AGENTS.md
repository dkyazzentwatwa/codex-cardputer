# Repository Guidelines

## Architecture

This repository contains a native SwiftUI menu bar companion, its bundled
Node.js/TypeScript bridge, and one Arduino CLI firmware target for the M5Stack
Cardputer ADV. The companion owns bridge lifecycle and desktop interaction. The
bridge owns Codex App Server integration and exposes only the constrained
`codexdeck.v1` WebSocket protocol to the device. The Cardputer never receives
Codex credentials, raw terminal output, environment snapshots, or an arbitrary
shell endpoint.

## Commands

```bash
pnpm install
pnpm lint
pnpm typecheck
pnpm test
pnpm mac:test
pnpm package:mac
arduino-cli compile --profile adv firmware/cardputer
./tools/flash-firmware.sh /dev/cu.usbmodemXXXX
```

Use Arduino CLI only for firmware. Do not add alternate embedded build-system
projects, cloud relays, online OTA, multi-board profiles, or direct App Server
LAN listeners.

## Safety Boundaries

- Target only M5Stack Cardputer ADV.
- Keep App Server on local stdio JSONL.
- Device mutations must be enumerated, validated, and idempotent.
- Device approval choices are limited to accept, decline, and cancel.
- Structured `request_user_input` requests remain desktop-only in v1.
- Keep management endpoints loopback-only and protected by a per-launch token.
- Treat the LAN protocol as unauthenticated and trusted-network-only.
- Report firmware states precisely as compile-ready, uploaded, or field-proven.

## Conventions

- TypeScript is strict and ESM-only.
- Validate external input at the boundary with Zod.
- Keep firmware buffers bounded and failures visible on both display and serial.
- Tests precede or accompany protocol, reducers, approvals, routing, and pure
  firmware logic.
