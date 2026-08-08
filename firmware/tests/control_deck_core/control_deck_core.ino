#include <ControlDeckCore.h>

using namespace codexdeck;

uint16_t passed = 0;
uint16_t failed = 0;

void check(bool condition, const char* name);
TaskState makeTask(const char* id, TaskStatus status, bool attention, const char* updated);

void check(bool condition, const char* name) {
  if (condition) {
    ++passed;
    Serial.printf("PASS %s\n", name);
  } else {
    ++failed;
    Serial.printf("FAIL %s\n", name);
  }
}

TaskState makeTask(const char* id, TaskStatus status, bool attention, const char* updated) {
  TaskState value;
  copyText(value.id, sizeof(value.id), id);
  copyText(value.title, sizeof(value.title), id);
  copyText(value.updatedAt, sizeof(value.updatedAt), updated);
  value.status = status;
  value.requiresAttention = attention;
  return value;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  TaskStore store;
  store.upsert(makeTask("run", TaskStatus::Running, false, "2026-07-15T10:00:00Z"));
  store.upsert(makeTask("wait", TaskStatus::WaitingApproval, true, "2026-07-15T09:00:00Z"));
  check(store.count() == 2, "task count");
  check(strcmp(store.at(0)->id, "wait") == 0, "attention sorting");
  store.select(1);
  store.upsert(makeTask("run", TaskStatus::Running, false, "2026-07-15T11:00:00Z"));
  check(strcmp(store.selected()->id, "run") == 0, "selection retention");
  check(store.selectById("wait") && strcmp(store.selected()->id, "wait") == 0, "selection by task id");
  store.markAllStale();
  check(store.find("run")->status == TaskStatus::Stale, "offline stale marking");

  TaskStore clearable;
  clearable.upsert(makeTask("done", TaskStatus::Completed, false, "2026-07-15T12:00:00Z"));
  clearable.upsert(makeTask("failed", TaskStatus::Failed, false, "2026-07-15T11:00:00Z"));
  clearable.upsert(makeTask("cancelled", TaskStatus::Cancelled, false, "2026-07-15T10:00:00Z"));
  clearable.upsert(makeTask("active", TaskStatus::Running, false, "2026-07-15T09:00:00Z"));
  clearable.upsert(makeTask("stale", TaskStatus::Stale, false, "2026-07-15T08:00:00Z"));
  check(clearable.clearableCount() == 3, "clearable terminal task count");
  check(clearable.selectById("active"), "select active before terminal removal");
  check(clearable.remove("done"), "remove finished task");
  check(strcmp(clearable.selected()->id, "active") == 0, "selection survives removal before selected task");
  check(clearable.remove("active"), "remove selected task");
  check(clearable.selected() != nullptr, "removing selected task chooses useful neighbor");

  check(keyboardShortcutCount() == 24, "keyboard shortcut catalog count");
  check(keyboardShortcutPageCount() == 3, "keyboard shortcut page count");
  check(strcmp(keyboardShortcutPageLabel(0), "TASKS") == 0 &&
            strcmp(keyboardShortcutPageLabel(1), "NAV") == 0 &&
            strcmp(keyboardShortcutPageLabel(2), "PANELS") == 0 &&
            strcmp(keyboardShortcutPageLabel(3), "") == 0,
        "keyboard shortcut page labels");
  check(keyboardShortcutPageItemCount(0) == 8, "keyboard shortcut first page count");
  check(keyboardShortcutPageItemCount(1) == 8 && keyboardShortcutPageItemCount(2) == 8,
        "keyboard shortcut full page counts");
  check(keyboardShortcutAt(keyboardShortcutCount()) == nullptr, "keyboard shortcut bounds");
  bool keyboardCatalogValid = true;
  bool keyboardIdsUnique = true;
  for (size_t left = 0; left < keyboardShortcutCount(); ++left) {
    const KeyboardShortcut* shortcut = keyboardShortcutAt(left);
    keyboardCatalogValid = keyboardCatalogValid && shortcut && keyboardShortcutValid(*shortcut);
    if (shortcut) {
      const size_t pageStart = keyboardShortcutPageStart(shortcut->page);
      const size_t pageCount = keyboardShortcutPageItemCount(shortcut->page);
      keyboardCatalogValid = keyboardCatalogValid && left >= pageStart && left < pageStart + pageCount;
    }
    for (size_t right = left + 1; right < keyboardShortcutCount(); ++right) {
      keyboardIdsUnique = keyboardIdsUnique && strcmp(shortcut->id, keyboardShortcutAt(right)->id) != 0;
    }
  }
  check(keyboardCatalogValid, "keyboard shortcut validation");
  check(keyboardIdsUnique, "keyboard shortcut ids unique");

  check(hidTransportValid(HidTransport::Usb) && hidTransportValid(HidTransport::Bluetooth),
        "HID transport validation");
  check(!hidTransportValid(static_cast<HidTransport>(2)), "invalid HID transport rejected");
  check(hidTransportFromStored(0) == HidTransport::Usb && hidTransportFromStored(1) == HidTransport::Bluetooth,
        "stored HID transport restored");
  check(hidTransportFromStored(255) == HidTransport::Usb, "invalid stored HID transport defaults to USB");
  check(strcmp(hidTransportLabel(HidTransport::Usb), "USB") == 0 &&
            strcmp(hidTransportLabel(HidTransport::Bluetooth), "BLUETOOTH") == 0,
        "HID transport labels");
  check(hidTransportActive(HidTransport::Usb, HidTransport::Usb) &&
            !hidTransportActive(HidTransport::Usb, HidTransport::Bluetooth) &&
            hidTransportActive(HidTransport::Bluetooth, HidTransport::Bluetooth) &&
            !hidTransportActive(HidTransport::Bluetooth, HidTransport::Usb),
        "HID routes to exactly one selected transport");

  check(deckThemeCount() == 5, "deck theme catalog count");
  check(strcmp(deckThemeLabel(DeckTheme::NeonGrid), "NEON GRID") == 0 &&
            strcmp(deckThemeLabel(DeckTheme::Terminal), "TERMINAL") == 0 &&
            strcmp(deckThemeLabel(DeckTheme::AmberCrt), "AMBER CRT") == 0 &&
            strcmp(deckThemeLabel(DeckTheme::Synthwave), "SYNTHWAVE") == 0 &&
            strcmp(deckThemeLabel(DeckTheme::Ice), "ICE") == 0,
        "deck theme labels");
  bool themesValid = true;
  for (size_t index = 0; index < deckThemeCount(); ++index) {
    themesValid = themesValid && deckThemeValid(deckThemeAt(index));
  }
  check(themesValid && !deckThemeValid(static_cast<DeckTheme>(deckThemeCount())), "deck theme validation");

  ApprovalController approval;
  check(!approval.press(1000), "first approval press arms");
  check(approval.press(2000), "second approval press confirms");
  check(!approval.press(3000), "approval rearms after submit");
  check(!approval.press(8001), "approval timeout blocks late press");
  check(approval.press(8002), "approval confirms after timeout rearm");
  approval.beginHold(3000);
  check(!approval.updateHold(4499), "early hold blocked");
  check(approval.updateHold(4500), "full hold confirms");
  approval.beginHold(5000);
  approval.cancelHold();
  check(!approval.updateHold(7000), "released hold does not confirm");

  check(reconnectDelayMs(0, 0) == 500, "initial reconnect delay");
  check(reconnectDelayMs(8, 0) == 30000, "reconnect delay cap");
  check(parseStatus("waiting_input") == TaskStatus::WaitingInput, "status parsing");
  check(!mutationAllowed(false), "offline mutation blocking");
  check(!mutationAllowed(true, false), "disabled macro blocking");
  check(mutationAllowed(true), "online mutation allowed");

  char bounded[5];
  copyText(bounded, sizeof(bounded), "123456789");
  check(strcmp(bounded, "1234") == 0, "bounded text truncation");

  Serial.printf("TEST SUMMARY passed=%u failed=%u\n", passed, failed);
}

void loop() { delay(1000); }
