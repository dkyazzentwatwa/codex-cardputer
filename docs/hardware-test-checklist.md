# Hardware test checklist

Record date, tester, Cardputer ADV serial, firmware commit, bridge commit, Codex
version, LAN details, and evidence links before changing any proof state.

## Proof state

- [x] `compile-ready`: production and logic-test sketches compile with the ADV
      Arduino CLI profile.
- [ ] `uploaded`: production and logic-test uploads have successful logs from a
      connected Cardputer ADV.
- [ ] `field-proven`: every applicable check below has device-facing evidence.

No Cardputer was connected during the initial implementation, so unchecked
items remain required.

## Upload and logic harness

- [ ] `arduino-cli board list` identifies the intended `/dev/cu.usbmodem*`.
- [ ] `./tools/test-firmware.sh PORT` re-detects the port after reset.
- [ ] Serial ends with `TEST SUMMARY ... failed=0`.
- [ ] `./tools/flash-firmware.sh PORT` reports a successful upload.
- [ ] Production serial prints the boot diagnostic without reset loops.

## Display and keyboard

- [ ] Display is landscape, 240 by 135, readable, and not clipped.
- [ ] Dark background, cyan selection, compact header/footer, and four task rows
      match the intended Cypher OS visual language.
- [ ] Dashboard status remains legible without relying only on color.
- [ ] Up, Down, Left, Right, Enter, Tab, Esc/back, Delete, 1 through 8, S, F,
      R, W, M, N, D, and K are verified in the keymap screen.
- [ ] Long titles, summaries, and toast messages remain bounded.

## USB and Bluetooth HID

- [ ] Existing USB shortcuts still work after upgrading, with USB selected by default.
- [ ] Bluetooth advertises only after Bluetooth is selected in HID settings.
- [ ] macOS pairs with `CodexDeck Cardputer` and the display reaches `BT READY`.
- [ ] Escape, Command, Control, Shift, Option, punctuation, and multi-modifier shortcuts work over BLE.
- [ ] Reboot restores the selected transport and reconnects the bonded Bluetooth host.
- [ ] Switching between USB and Bluetooth sends each shortcut to only the selected host.
- [ ] Confirmed Clear BT Pairing removes the bond and requires a fresh macOS pairing.
- [ ] Wi-Fi, bridge reconnection, and BLE HID remain stable together without reset loops or unsafe heap loss.
- [ ] Repeat the applicable checks on the Waveshare AMOLED profile before calling its Bluetooth HID field-proven.

## Wi-Fi and discovery

- [ ] First boot scans networks and masks password entry.
- [ ] Manual hidden SSID entry connects successfully.
- [ ] Credentials survive reboot in ESP32 Preferences.
- [ ] `_codexdeck._tcp.local` finds the bridge without manual IP entry.
- [ ] Saved-host fallback works when mDNS is temporarily unavailable.
- [ ] Wi-Fi loss shows separate Wi-Fi and bridge states.
- [ ] Reconnection occurs without reboot and mutations stay blocked offline.

## Acceptance scenarios

- [ ] A: bridge restart shows stale/offline state, reconnects, and restores the
      full snapshot within ten seconds on the test LAN.
- [ ] B: one workflow launch creates one thread; a duplicate request ID creates
      no second thread.
- [ ] C: three concurrent tasks sort correctly and render without raw output.
- [ ] D: normal approval arms on first Enter and submits only on the second
      Enter within five seconds.
- [ ] E: high-risk approval is labeled, ignores a tap, and submits only after a
      continuous 1.5 second hold.
- [ ] F: stopping one selected task cancels the correct turn and leaves the
      other two unaffected.
- [ ] G: a follow-up returns the correct thread to running.
- [ ] H: malformed, oversized, unknown, duplicated, and invalid-ID traffic does
      not crash either endpoint or expose an arbitrary command path.

## Recovery and soak

- [ ] App Server exit marks tasks stale, restarts, and resumes managed threads.
- [ ] Bridge process restart restores compact task mappings from atomic state.
- [ ] Repeated Wi-Fi loss and reconnect preserves selection where possible.
- [ ] Thirty-minute soak runs with at least three concurrent tasks.
- [ ] Heap, last bridge message age, Wi-Fi, socket, and task count remain visible
      on Diagnostics.

## Evidence summary

| Gate                         | Result                          | Evidence               |
| ---------------------------- | ------------------------------- | ---------------------- |
| Automated bridge tests       | Pass, 27 bridge plus 8 protocol | `docs/verification.md` |
| Production firmware compile  | Pass                            | `docs/verification.md` |
| Logic test sketch compile    | Pass                            | `docs/verification.md` |
| Logic test sketch upload/run | Blocked: no device connected    |                        |
| Production upload            | Blocked: no device connected    |                        |
| Field acceptance A through H | Blocked: no device connected    |                        |
| 30-minute soak               | Blocked: no device connected    |                        |
