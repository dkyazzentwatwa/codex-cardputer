# CardPuter Codex Control Deck

A trusted-LAN control surface for bridge-managed local Codex tasks. The desktop
bridge owns `codex app-server`; the M5Stack Cardputer ADV receives only compact
task state and a constrained `codexdeck.v1` action protocol.

> [!WARNING]
> Version 1 has no device authentication. Run it only on a trusted, isolated
> private LAN. Never expose port `8765` through a tunnel, public listener, port
> forward, or cloud relay.

## What works

- Launch allowlisted workflows and enabled Codex skills.
- Monitor up to 20 bridge-managed tasks, four rows at a time.
- Stop one turn, steer a running turn, or start a follow-up turn.
- Route command and file-change approvals to every connected device.
- Require two presses for normal approval and a 1.5 second hold for high risk.
- Show `request_user_input` as `waiting_input`; answers stay on the desktop.
- Discover the bridge over mDNS and fall back to the last saved private host.
- Configure Wi-Fi from the Cardputer keyboard, including hidden SSIDs.
- Recover from App Server, bridge, WebSocket, and Wi-Fi interruption.

Tasks created outside this bridge are not monitored in v1.

## Requirements

- Node.js 20 or newer
- pnpm 11
- Codex CLI 0.140.0, the initially tested version
- Arduino CLI
- M5Stack board index installed in Arduino CLI
- M5Stack Cardputer ADV for upload and field verification

## Desktop setup

```bash
pnpm install
cp apps/bridge/config/bridge.example.yaml apps/bridge/config/bridge.local.yaml
cp apps/bridge/config/workflows.example.yaml apps/bridge/config/workflows.local.yaml
```

Edit `workflows.local.yaml`. Every `cwd` must be an existing absolute path.
Then point `bridge.local.yaml` at it and run:

```bash
./tools/start-bridge.sh
```

The helper resolves the local configuration to an absolute path before pnpm
enters the bridge package directory.

With no `bindHost`, the bridge deterministically chooses the first private IPv4
interface and falls back to `127.0.0.1` when no private interface exists. Use an
explicit `bindHost: 127.0.0.1` for local-only development.

Check bridge health at `http://HOST:8765/healthz`. Send `SIGHUP` to reload the
allowlist and enabled skill menu without restarting active tasks.

## Firmware setup

This repository uses Arduino CLI only. The `adv` profile pins the Cardputer ADV
FQBN and all required libraries in `firmware/cardputer/sketch.yaml`.

```bash
arduino-cli compile --profile adv firmware/cardputer
arduino-cli compile --profile adv firmware/tests/control_deck_core
```

Connect the Cardputer over USB, verify its port, then upload:

```bash
arduino-cli board list
./tools/flash-firmware.sh /dev/cu.usbmodemXXXX
```

The first boot opens Wi-Fi setup. Choose a network, or press `M` for a hidden
SSID. Passwords are masked and saved in ESP32 Preferences. The offline screen
can reopen setup with `W`.

Run the on-device assertion sketch with:

```bash
./tools/test-firmware.sh /dev/cu.usbmodemXXXX
```

The helper compiles, uploads, re-detects the USB modem port, monitors serial via
Arduino CLI, and succeeds only after `TEST SUMMARY ... failed=0`.

## Verification

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

See [architecture](docs/architecture.md), [protocol](docs/protocol.md),
[security](docs/security.md), [troubleshooting](docs/troubleshooting.md), and the
[hardware checklist](docs/hardware-test-checklist.md). The latest local gate is
recorded in [verification](docs/verification.md).

## Proof state

The repository can be `compile-ready` without hardware. `uploaded` requires a
successful Arduino CLI upload log. `field-proven` requires the completed device
checklist and recorded acceptance evidence. These states are never treated as
interchangeable.
