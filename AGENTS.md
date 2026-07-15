# Repository Guidelines

## Architecture

This repository contains a Node.js/TypeScript desktop bridge and one Arduino
CLI firmware target for the M5Stack Cardputer ADV. The bridge owns Codex App
Server integration and exposes only the constrained `codexdeck.v1` WebSocket
protocol to the device. The Cardputer never receives Codex credentials, raw
terminal output, environment snapshots, or an arbitrary shell endpoint.

## Commands

```bash
pnpm install
pnpm lint
pnpm typecheck
pnpm test
arduino-cli compile --profile adv firmware/cardputer
./tools/flash-firmware.sh /dev/cu.usbmodemXXXX
```

Use Arduino CLI only for firmware. Do not add PlatformIO, ESP-IDF project
files, cloud relays, online OTA, multi-board profiles, or direct App Server LAN
listeners.

## Safety Boundaries

- Target only M5Stack Cardputer ADV.
- Keep App Server on local stdio JSONL.
- Device mutations must be enumerated, validated, and idempotent.
- Device approval choices are limited to accept, decline, and cancel.
- Structured `request_user_input` requests remain desktop-only in v1.
- Treat the LAN protocol as unauthenticated and trusted-network-only.
- Report firmware states precisely as compile-ready, uploaded, or field-proven.

## Conventions

- TypeScript is strict and ESM-only.
- Validate external input at the boundary with Zod.
- Keep firmware buffers bounded and failures visible on both display and serial.
- Tests precede or accompany protocol, reducers, approvals, routing, and pure
  firmware logic.
