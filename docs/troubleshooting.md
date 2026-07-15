# Troubleshooting

## Bridge reports degraded health

Run `codex --version` and `pnpm codex:compat`. The bridge requires a working
local Codex CLI and starts exactly `codex app-server --listen stdio://`.
Warnings on App Server stderr are logged but do not by themselves mean the
adapter is unavailable. Check `codexReady` in `/healthz`.

## Cardputer has Wi-Fi but cannot find the bridge

Confirm both devices are on the same 2.4 GHz LAN and that client isolation is
off. Verify the bridge startup address is private and reachable. mDNS may be
blocked on segmented networks; after one successful discovery the firmware
keeps the last host as a fallback. Press `R` on the offline screen to retry.

For local diagnostics, set `bindHost` explicitly and check:

```bash
curl http://HOST:8765/healthz
```

## Wi-Fi credentials need changing

Press `W` on the offline screen. Select a scanned network, press `M` for a
hidden SSID, or `R` to rescan. Password text is masked. Saving a new network
replaces the prior ESP32 Preferences values.

## A request waits for desktop input

This is expected for v1. The Cardputer shows `INPUT`; answer the
`request_user_input` prompt in the bridge terminal. No structured answer is
sent from the device.

## Approval does not submit

Normal approval requires the same decision twice within five seconds.
High-risk acceptance requires holding Enter continuously for 1.5 seconds;
releasing early cancels the local confirmation. If the bridge disconnected,
all mutations stay disabled until a new `welcome` handshake.

## Firmware compile fails

Use Arduino CLI from the repository root and do not substitute another firmware
build system:

```bash
arduino-cli compile --profile adv firmware/cardputer
```

The profile in `sketch.yaml` installs the M5Stack platform and pinned libraries.
If board packages are unavailable, verify the M5Stack package index is reachable
and rerun the same command.

## Upload helper finds no port

Run `arduino-cli board list`, reconnect the USB cable, and use a data-capable
cable. The helper accepts `/dev/cu.usbmodem*`, triggers the bootloader, then
re-detects the port before upload. No successful compile implies upload proof.

## Device input differs from labels

Open Diagnostics with `D`, then Keymap with `K`. The diagnostic view shows
normalized text and direction events. Cardputer arrow, Tab, Esc, and Delete
behavior must be verified on hardware before claiming field proof.
