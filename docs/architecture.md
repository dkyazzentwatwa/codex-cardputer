# Architecture

The Cardputer is a bounded display and input client. The desktop bridge owns
Codex App Server, task state, workflow validation, approvals, persistence,
redaction, and LAN policy. App Server remains a local stdio child process.

The device connects only to `/device` using `codexdeck.v1` JSON text frames.
There is no arbitrary command message and no raw Codex output path.
