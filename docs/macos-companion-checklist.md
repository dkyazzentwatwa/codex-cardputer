# macOS companion acceptance checklist

Use this checklist after opening `/Applications/CodexDeck.app`.

## First launch

- The CodexDeck icon appears in the menu bar and no permanent Dock icon remains.
- The onboarding view explains the trusted-LAN boundary.
- Codex CLI version and sign-in state are visible.
- `littlehakr`, `ai-b2b`, and `codex-cardputer` are listed as available projects.
- Finish Setup starts the bridge and the icon changes from gray through amber to cyan.

## Everyday control

- The menu shows the LAN address, Codex readiness, Cardputer count, and active task count.
- Start, Stop, and Restart change state without opening Terminal.
- Start at Login can be enabled and disabled from General settings.
- Adding a project uses a folder picker and rejects nonexistent paths.
- Editing a workflow updates the Cardputer macro snapshot after Save and Reload.
- Diagnostics shows recent structured logs and Copy Diagnostics produces a redacted summary.

## Attention and recovery

- `request_user_input` makes the icon amber and opens a native answer form.
- Normal approvals show Accept, Allow for Session when supported, Decline, and Cancel Task.
- High-risk acceptance requires a second destructive confirmation.
- A bridge crash retries after 1, 2, 5, then 15 seconds and becomes red after the retry limit.
- An independently started bridge is reported without terminating its process.
- Wi-Fi or Cardputer loss changes device state without disabling local bridge controls.

## Proof language

Record Mac states separately as `build-ready`, `development-signed`,
`notarized`, and `locally launched`. Record firmware separately as
`compile-ready`, `uploaded`, and `field-proven`.
