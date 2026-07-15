# Verification record

Date: 2026-07-15

## Automated gate

`pnpm verify` passed from the repository root.

- ESLint: pass
- Prettier check: pass
- TypeScript typecheck: pass
- Protocol tests: 8 passed
- Bridge tests: 27 passed across 13 files
- TypeScript production build: pass
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

## Hardware gate

`arduino-cli board list` found no `/dev/cu.usbmodem*` Cardputer. Therefore:

- proof state is `compile-ready`;
- production firmware is not `uploaded`;
- the serial assertion harness is not device-run;
- acceptance scenarios A through H and the 30-minute soak are not
  `field-proven`.

Complete `docs/hardware-test-checklist.md` when a Cardputer ADV is connected.
