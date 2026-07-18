# Verification record

Date: 2026-07-15

## Automated gate

`pnpm verify` passed from the repository root.

- ESLint: pass
- Prettier check: pass
- TypeScript typecheck: pass
- Protocol tests: 8 passed
- Bridge tests: 34 passed across 17 files
- TypeScript production build: pass
- Swift companion tests: 6 passed
- Swift production build: pass for Apple Silicon, macOS 13 minimum
- Development code signing: pass for the app and bundled Node helper
- Packaged-app smoke: pass with a Finder-like empty PATH
- Self-contained archive: `CodexDeck-0.1.0-arm64.zip`, 41 MB
- Installed Codex App Server schema generation: pass
- Narrow adapter compatibility against generated Codex TypeScript: pass
- Production Cardputer ADV compile: pass, 1,341,991 bytes flash (40%),
  165,196 bytes static RAM (50%)
- Arduino logic-test compile: pass, 286,471 bytes flash (8%), 21,572 bytes
  static RAM (6%)

## Live desktop smoke

The bridge started a real local `codex app-server --listen stdio://` using Codex
CLI 0.140.0. App Server initialized, 20 enabled skills were discovered for the
allowlisted project, `/healthz` returned `status: ok` and `codexReady: true`, and
a real `/device` WebSocket handshake received a 20-entry macro snapshot with
both `launch_workflow` and `launch_skill` actions.

Local plugin-manifest warnings appeared on App Server stderr but did not prevent
initialization or skill discovery.

## Menu bar companion

The app-managed configuration migration created mode `0600` JSON files under
Application Support and retained all three configured projects: `littlehakr`,
`ai-b2b`, and `codex-cardputer`. The original local YAML files were unchanged.

The release assembly bundled Node 24.15.0, compiled bridge JavaScript,
production dependencies, schemas, and offline documentation. Strict deep
code-sign verification passed. The app launched and remained alive with an
empty inherited shell PATH, proving that normal launch does not depend on the
development Node or pnpm commands being discoverable.

The development-signed beta is installed at `/Applications/CodexDeck.app` and
is currently visible as a menu bar app. It is not notarized and is not yet a
public-distribution build.

## Hardware gate

`arduino-cli board list` found no `/dev/cu.usbmodem*` Cardputer. Therefore:

- proof state is `compile-ready`;
- Mac companion proof state is `development-signed` and `locally launched`;
- Mac companion is not `notarized`;
- production firmware is not `uploaded`;
- the serial assertion harness is not device-run;
- acceptance scenarios A through H and the 30-minute soak are not
  `field-proven`.

Complete `docs/hardware-test-checklist.md` when a Cardputer ADV is connected.
