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

## Future compatibility

Authentication can be added to the handshake in a future protocol version.
Message semantics are versioned so a v2 authentication layer need not widen
the existing action surface.
