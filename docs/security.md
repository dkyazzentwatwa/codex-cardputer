# Security model

Version 1 is intentionally unauthenticated on a trusted local network. Any
client able to reach the bridge can attempt an enumerated protocol action. This
is low-friction and trust-based, not secure against a hostile LAN participant.

## Required deployment boundary

- Use a private, controlled Wi-Fi network.
- Never add public port forwarding, a cloud host, or a tunnel to port 8765.
- Use `bindHost: 127.0.0.1` when no LAN device access is required.
- Treat a shared guest, conference, hotel, or cafe network as untrusted.
- Keep Codex sandbox and approval policy enabled. Device confirmation is an
  extra UI safeguard, not an authorization boundary.

## Data minimization

The bridge sends compact state, not raw Codex output. It removes control codes,
redacts common token and secret forms, shortens paths, and bounds every field.
The Cardputer persists only Wi-Fi credentials and a last bridge hint. It does
not store Codex credentials, prompts, task history, project paths, approvals,
or command output.

The bridge state file contains bridge-managed thread and task metadata. It is
written atomically with mode `0600`. Resolved approval payloads and follow-up
history are not persisted.

Companion settings, workflows, state, and logs live under the user's
Application Support directory. JSON files use mode `0600`; the directory uses
mode `0700`. The app never copies or stores Codex credentials.

## Bluetooth HID boundary

Bluetooth HID is separate from the CodexDeck LAN protocol. It sends only the
same fixed keyboard shortcuts available over USB and never carries task state,
Codex credentials, bridge messages, or terminal output. USB is the default
transport. BLE advertising starts only when the user selects Bluetooth in the
local HID settings, and shortcut reports go to exactly one selected transport.

Bluetooth uses bonded LE Secure Connections with Just Works pairing. This is
convenient but does not authenticate the nearby host with a passcode. Pair only
in a controlled location, verify the `CodexDeck Cardputer` or
`CodexDeck AMOLED` device name, and use the confirmed Clear BT Pairing action
before transferring the device or when a host should no longer reconnect.

## Constrained actions

YAML defines allowlisted projects, workflows, Codex settings, and prompts. It
requires existing absolute project paths, rejects unknown fields, and does not
perform environment expansion or shell interpolation. Skills come only from
the enabled list reported by App Server for an allowlisted project.

Device approvals expose only `accept`, `decline`, and `cancel`. Session-wide
approval, policy amendment, arbitrary commands, and structured
`request_user_input` answers remain desktop-only.

High-risk classification highlights destructive deletion, privilege elevation,
Git history rewrite, credential stores, disk and power commands, system network
changes, and package publication. Normal acceptance requires two Enter presses
within five seconds. High-risk acceptance requires a continuous 1.5 second
hold. Codex remains the source of truth for whether an action requires approval.

## Companion management boundary

The app-management server is separate from port 8765. It always binds to
`127.0.0.1` on an ephemeral port and requires a randomly generated 256-bit
bearer token on HTTP and WebSocket requests. The token is passed only through
the child-process environment, never written to disk or logs, and disappears
when either process exits. No management endpoint is advertised through mDNS.

The directly distributed companion is intentionally not App Sandbox enabled
because Codex must launch subprocesses and access the explicitly allowlisted
project folders. Hardened runtime is enabled. The bundled Node helper is signed
with the JIT entitlements required by V8.

## Future compatibility

Authentication can be added to the handshake in a future protocol version.
Message semantics are versioned so a v2 authentication layer need not widen
the existing action surface.
