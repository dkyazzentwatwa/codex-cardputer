# Troubleshooting

## Bridge reports degraded health

Open CodexDeck from the menu bar and check Diagnostics. The app shows Codex
readiness, the listening address, connected devices, recent structured logs,
and the last bridge error. Use Restart after correcting the configuration.

For terminal diagnosis, run `codex --version` and `pnpm codex:compat`. The bridge
requires a working local Codex CLI and starts exactly
`codex app-server --listen stdio://`.
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

This is expected for v1. The Cardputer shows `INPUT`; click the amber CodexDeck
menu bar icon and answer the native prompt. No structured answer is sent from
the device. Terminal-launched bridges retain the terminal input fallback.

## Another bridge is already running

CodexDeck never kills a process it does not own. When a matching health endpoint
is detected, the app reports an external bridge and provides view-only status.
Stop that bridge in its original terminal, choose another port, or retry.

## Codex is installed but the app cannot find it

Finder-launched apps do not inherit the shell's full PATH. CodexDeck searches
Homebrew, NVM versions, and the login shell. In General settings, use Choose to
select the executable directly. The saved absolute path is used for later
launches.

## Start at Login is denied

Open System Settings, General, Login Items and confirm CodexDeck is allowed.
Then toggle Start companion at login again in CodexDeck General settings.

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
