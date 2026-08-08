#include "ui_renderer.h"

#include <M5Cardputer.h>
#include <new>

namespace {
struct UiPalette {
  uint16_t bg;
  uint16_t header;
  uint16_t footer;
  uint16_t panel;
  uint16_t panelAlt;
  uint16_t grid;
  uint16_t text;
  uint16_t dim;
  uint16_t accent;
  uint16_t violet;
  uint16_t good;
  uint16_t warn;
  uint16_t bad;
};

UiPalette activePalette = {0x0009, 0x0821, 0x0420, 0x0842, 0x1084, 0x0843, 0xFFFF,
                           0x8410, 0x07FF, 0xA19F, 0x07E0, 0xFFE0, 0xF800};

#define BG activePalette.bg
#define HEADER activePalette.header
#define FOOTER activePalette.footer
#define PANEL activePalette.panel
#define PANEL_ALT activePalette.panelAlt
#define GRID activePalette.grid
#define TEXT activePalette.text
#define DIM activePalette.dim
#define ACCENT activePalette.accent
#define CYBER_VIOLET activePalette.violet
#define GOOD activePalette.good
#define WARN activePalette.warn
#define BAD activePalette.bad

UiPalette paletteFor(codexdeck::DeckTheme theme) {
  switch (theme) {
    case codexdeck::DeckTheme::Terminal:
      return {0x0000, 0x0200, 0x0100, 0x0200, 0x0320, 0x0100, 0xCFE0,
              0x5BE0, 0x07E0, 0x03C0, 0x07E0, 0xFD20, 0xF800};
    case codexdeck::DeckTheme::AmberCrt:
      return {0x0000, 0x5200, 0x2100, 0x2940, 0x4200, 0x2100, 0xFFE0,
              0xCBE0, 0xFD20, 0xCB20, 0xBFE0, 0xFFE0, 0xF800};
    case codexdeck::DeckTheme::Synthwave:
      return {0x1008, 0x4816, 0x280B, 0x2811, 0x4012, 0x2009, 0xFFFF,
              0xC79F, 0xF81F, 0x07FF, 0x07E0, 0xFFE0, 0xF800};
    case codexdeck::DeckTheme::Ice:
      return {0x0012, 0x039F, 0x0210, 0x18D6, 0x18D6, 0x086B, 0xFFFF,
              0xBDF7, 0x7DFF, 0x3D9F, 0x07E0, 0xFFE0, 0xF800};
    case codexdeck::DeckTheme::NeonGrid:
    default:
      return {0x0009, 0x0821, 0x0420, 0x0842, 0x1084, 0x0843, 0xFFFF,
              0x8410, 0x07FF, 0xA19F, 0x07E0, 0xFFE0, 0xF800};
  }
}

uint16_t statusColor(codexdeck::TaskStatus status) {
  if (status == codexdeck::TaskStatus::Completed) return GOOD;
  if (status == codexdeck::TaskStatus::Failed || status == codexdeck::TaskStatus::Cancelled) return BAD;
  if (status == codexdeck::TaskStatus::WaitingApproval || status == codexdeck::TaskStatus::WaitingInput) return WARN;
  if (status == codexdeck::TaskStatus::Stale) return DIM;
  return TEXT;
}

const char* nextWrappedLine(const char* cursor, uint8_t maxChars, char* output, size_t outputCapacity) {
  if (!cursor || !*cursor) return cursor;
  const char* start = cursor;
  const char* lastSpace = nullptr;
  size_t length = 0;
  while (*cursor && *cursor != '\n' && length < maxChars) {
    if (*cursor == ' ') lastSpace = cursor;
    ++cursor;
    ++length;
  }
  if (*cursor && *cursor != '\n' && lastSpace && lastSpace > start) {
    length = static_cast<size_t>(lastSpace - start);
    cursor = lastSpace + 1;
  } else if (*cursor == '\n') {
    ++cursor;
  }
  if (output && outputCapacity) {
    const size_t copied = min(length, outputCapacity - 1);
    memcpy(output, start, copied);
    output[copied] = '\0';
  }
  return cursor;
}

size_t wrappedLineCount(const char* text, uint8_t maxChars) {
  size_t lines = 0;
  const char* cursor = text ? text : "";
  while (*cursor) {
    cursor = nextWrappedLine(cursor, maxChars, nullptr, 0);
    ++lines;
  }
  return lines;
}
}  // namespace

void UiRenderer::setTheme(codexdeck::DeckTheme theme) { activePalette = paletteFor(theme); }

void UiRenderer::begin() {
  M5Cardputer.Display.setRotation(1);
  // Keep startup unambiguous on a freshly flashed device. The Cardputer's
  // display backlight can retain its prior state across resets.
  M5Cardputer.Display.setBrightness(255);
  M5Cardputer.Display.fillScreen(BG);

  // Create the canvas only after M5Cardputer has initialized its display.
  // Constructing it as a global member can race the library's global display
  // constructor and prevent the ESP32 from reaching setup().
  if (ESP.getPsramSize() > 0) canvas_ = new (std::nothrow) M5Canvas(&M5Cardputer.Display);
  if (canvas_) {
    canvas_->setPsram(true);
    canvas_->setColorDepth(16);
    buffered_ = canvas_->createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height()) != nullptr;
  }
  auto& display = surface();
  display.setTextDatum(top_left);
  display.setTextWrap(false);
  display.setTextSize(1);
  display.fillScreen(BG);
  logDiagnostics("init");
}

void UiRenderer::boot(const char* status, uint8_t progress) {
  auto& display = surface();
  display.fillScreen(BG);
  const int16_t width = display.width();
  const int16_t height = display.height();
  const uint32_t tick = millis() / 80;

  for (int16_t y = 8; y < height - 8; y += 16) display.drawFastHLine(8, y, width - 16, 0x0841);
  for (int16_t x = 12; x < width - 8; x += 24) display.drawFastVLine(x, 8, height - 16, 0x0421);
  for (uint8_t index = 0; index < 12; ++index) {
    const int16_t x = (13 + index * 19 + tick * (index % 3 + 1)) % (width - 16) + 8;
    const int16_t y = 17 + (index * 29) % (height - 34);
    display.drawPixel(x, y, index % 3 ? ACCENT : TEXT);
  }
  display.drawRect(4, 4, width - 8, height - 8, HEADER);
  display.drawRect(7, 7, width - 14, height - 14, ACCENT);
  display.fillRect(16, 24, width - 32, 42, BG);
  display.setTextColor(ACCENT, BG);
  display.setTextSize(2);
  display.setCursor(58, 29);
  display.print("CODEX");
  display.setTextColor(TEXT, BG);
  display.setCursor(70, 48);
  display.print("DECK");
  display.setTextSize(1);
  display.setTextColor(DIM, BG);
  display.setCursor(67, 72);
  display.print("LOCAL COMPANION");
  display.drawRoundRect(61, 87, 118, 14, 4, ACCENT);
  display.setTextColor(ACCENT, BG);
  display.setCursor(70, 91);
  display.print("CARDPUTER ADV");
  display.drawRect(16, 110, width - 32, 7, DIM);
  const int16_t fill = ((width - 34) * min<uint8_t>(progress, 100)) / 100;
  display.fillRect(17, 111, fill, 5, ACCENT);
  display.setTextColor(TEXT, BG);
  display.setCursor(16, 122);
  display.print(status ? status : "Starting...");
  display.setTextColor(DIM, BG);
  display.setCursor(width - 39, 122);
  display.printf("%3u%%", min<uint8_t>(progress, 100));
  present();
}

void UiRenderer::dashboard(const codexdeck::TaskStore& tasks, bool connected, bool hidReady, const char* toast) {
  size_t running = 0;
  size_t waiting = 0;
  for (size_t index = 0; index < tasks.count(); ++index) {
    const codexdeck::TaskState* task = tasks.at(index);
    if (task->status == codexdeck::TaskStatus::Running) ++running;
    if (task->requiresAttention) ++waiting;
  }
  header("CODEX DECK", connected ? "LIVE" : "OFFLINE");
  auto& display = surface();
  metricChip(5, 25, 72, "ACTIVE", String(running).c_str(), GOOD);
  metricChip(84, 25, 72, "ATTN", String(waiting).c_str(), waiting ? WARN : DIM);
  metricChip(163, 25, 72, "HID", hidReady ? "LIVE" : "WAIT", hidReady ? ACCENT : DIM);
  if (tasks.count() == 0) {
    panel(22, 53, 196, 52, connected ? ACCENT : WARN, PANEL, 7);
    display.drawCircle(120, 70, 9, connected ? ACCENT : WARN);
    display.drawFastHLine(107, 70, 26, connected ? ACCENT : WARN);
    display.drawFastVLine(120, 57, 26, connected ? ACCENT : WARN);
    display.setTextColor(TEXT, PANEL);
    display.setCursor(68, 84);
    display.print(connected ? "NO ACTIVE TASKS" : "BRIDGE OFFLINE");
    display.setTextColor(DIM, PANEL);
    display.setCursor(43, 95);
    display.print(connected ? "Launch a workflow to begin" : "Reconnect from the deck menu");
  } else {
    const size_t selected = tasks.selectedIndex();
    const size_t scroll = selected >= 3 ? selected - 2 : 0;
    for (size_t visible = 0; visible < 3 && scroll + visible < tasks.count(); ++visible) {
      const codexdeck::TaskState* task = tasks.at(scroll + visible);
      const int16_t y = 50 + static_cast<int16_t>(visible) * 21;
      const bool selectedCard = scroll + visible == selected;
      const uint16_t state = statusColor(task->status);
      panel(5, y, 230, 18, selectedCard ? ACCENT : PANEL_ALT, selectedCard ? 0x106D : PANEL, 5);
      display.fillCircle(15, y + 9, 3, state);
      if (selectedCard) display.drawCircle(15, y + 9, 5, ACCENT);
      display.setTextColor(TEXT, selectedCard ? 0x106D : PANEL);
      display.setCursor(25, y + 3);
      display.print(String(task->title).substring(0, 22));
      const String stateLabel(codexdeck::statusLabel(task->status));
      const int16_t badgeWidth = static_cast<int16_t>(stateLabel.length() * 6 + 8);
      display.fillRoundRect(230 - badgeWidth, y + 4, badgeWidth, 10, 3, selectedCard ? BG : PANEL_ALT);
      display.setTextColor(selectedCard ? ACCENT : state, selectedCard ? BG : PANEL_ALT);
      display.setCursor(232 - badgeWidth, y + 6);
      display.print(stateLabel);
    }
  }
  if (toast && toast[0]) {
    panel(34, 109, 172, 10, ACCENT, PANEL, 3);
    display.setTextColor(ACCENT, PANEL);
    display.setCursor(40, 111);
    display.print(String(toast).substring(0, 38));
  }
  dashboardFooter();
}

void UiRenderer::keyboard(const char* hidStatus, size_t selected, size_t page, const char* toast) {
  char indicator[12];
  snprintf(indicator, sizeof(indicator), "%u/%u", page + 1, codexdeck::keyboardShortcutPageCount());
  header("MACRO PAD", indicator);
  const size_t start = codexdeck::keyboardShortcutPageStart(page);
  const size_t count = codexdeck::keyboardShortcutPageItemCount(page);
  auto& display = surface();
  const bool ready = hidStatus && (strcmp(hidStatus, "USB READY") == 0 || strcmp(hidStatus, "BT READY") == 0);
  display.setTextColor(CYBER_VIOLET, BG);
  display.setCursor(7, 25);
  display.print(codexdeck::keyboardShortcutPageLabel(page));
  panel(163, 23, 72, 13, ready ? GOOD : WARN, PANEL, 4);
  display.fillCircle(171, 29, 2, ready ? GOOD : WARN);
  display.setTextColor(ready ? GOOD : WARN, PANEL);
  display.setCursor(177, 26);
  display.print(hidStatus ? hidStatus : "HID WAIT");
  for (size_t item = 0; item < count; ++item) {
    const codexdeck::KeyboardShortcut* shortcut = codexdeck::keyboardShortcutAt(start + item);
    if (!shortcut) continue;
    const int16_t column = static_cast<int16_t>(item % 2);
    const int16_t row = static_cast<int16_t>(item / 2);
    const int16_t x = 5 + column * 116;
    const int16_t y = 40 + row * 20;
    const bool active = start + item == selected;
    const uint16_t border = active ? ACCENT : PANEL_ALT;
    const uint16_t background = active ? 0x106D : PANEL;
    panel(x, y, 110, 18, border, background, 5);
    keycap(x + 5, y + 4, String(item + 1).c_str(), active);
    display.setTextColor(active ? TEXT : TEXT, background);
    display.setCursor(x + 24, y + 3);
    display.print(String(shortcut->label).substring(0, 14));
    display.setTextColor(active ? ACCENT : DIM, background);
    const String combo(shortcut->combo);
    display.setCursor(x + 24, y + 11);
    display.print(combo);
  }
  if (toast && toast[0]) {
    panel(36, 62, 168, 26, GOOD, 0x0842, 6);
    display.setTextColor(GOOD, 0x0842);
    display.setCursor(48, 68);
    display.print("ACTION SENT");
    display.setTextColor(TEXT, 0x0842);
    display.setCursor(48, 78);
    display.print(String(toast).substring(0, 23));
  }
  macroFooter();
}

void UiRenderer::taskDetail(const codexdeck::TaskState& task, size_t selectedMacro, size_t scrollLine) {
  constexpr uint8_t visibleLines = 6;
  const char* text = task.detail[0] ? task.detail : task.summary;
  const size_t totalLines = max<size_t>(1, wrappedLineCount(text, 37));
  const size_t maximumScroll = totalLines > visibleLines ? totalLines - visibleLines : 0;
  const size_t firstLine = min(scrollLine, maximumScroll);
  char indicator[18];
  snprintf(indicator, sizeof(indicator), "%u-%u/%u", firstLine + 1,
           min(firstLine + visibleLines, totalLines), totalLines);
  header(task.title, indicator);
  auto& display = surface();
  display.setTextColor(statusColor(task.status), BG);
  display.setCursor(8, 28);
  display.printf("%s  %02lu:%02lu", codexdeck::statusLabel(task.status),
                 task.elapsedSeconds / 60, task.elapsedSeconds % 60);
  wrappedWindow(text, 8, 43, 37, visibleLines, firstLine, TEXT);
  if (task.macroCount) {
    const codexdeck::MacroState& macro = task.macros[min(selectedMacro, task.macroCount - 1)];
    display.setTextColor(ACCENT, BG);
    display.setCursor(8, 109);
    display.printf("> %s", macro.shortLabel);
  }
  footer("Up/Down read  Left/Right action");
}

size_t UiRenderer::taskLineCount(const codexdeck::TaskState& task) const {
  return wrappedLineCount(task.detail[0] ? task.detail : task.summary, 37);
}

void UiRenderer::macros(const ControlDeckClient& client, size_t selected, size_t page) {
  char indicator[18];
  const size_t pages = (client.globalMacroCount() + 7) / 8;
  snprintf(indicator, sizeof(indicator), "%u/%u", page + 1, pages == 0 ? 1 : pages);
  header("WORKFLOWS + SKILLS", indicator);
  const size_t start = page * 8;
  if (client.globalMacroCount() == 0) {
    wrapped("No configured workflows or enabled Codex skills were found.", 8, 38, 37, 5, DIM);
  }
  for (size_t item = 0; item < 8 && start + item < client.globalMacroCount(); ++item) {
    const codexdeck::MacroState* macro = client.globalMacro(start + item);
    const int16_t y = 23 + item * 12;
    const bool active = start + item == selected;
    const uint16_t bg = active ? ACCENT : BG;
    auto& display = surface();
    display.fillRect(4, y, 232, 11, bg);
    display.setTextColor(active ? BG : (macro->enabled ? TEXT : DIM), bg);
    display.setCursor(7, y + 2);
    display.printf("%u %s", item + 1, macro->label);
  }
  footer("1-8 launch  </> page  ` back");
}

void UiRenderer::approval(const ApprovalState& approval, const char* armedDecision, bool armed, uint8_t holdPercent) {
  const bool high = strcmp(approval.risk, "high") == 0;
  header(high ? "HIGH RISK APPROVAL" : approval.title, approval.risk);
  wrapped(approval.summary, 8, 27, 37, 3, high ? WARN : TEXT);
  if (approval.command[0]) wrapped(approval.command, 8, 62, 37, 2, DIM);
  if (approval.riskReason[0]) wrapped(approval.riskReason, 8, 86, 37, 1, WARN);
  if (high) {
    surface().drawRect(10, 101, 220, 10, DIM);
    surface().fillRect(11, 102, (218 * holdPercent) / 100, 8, BAD);
  } else if (armed) {
    surface().setTextColor(WARN, BG);
    surface().setCursor(8, 102);
    surface().printf("PRESS %s AGAIN", armedDecision);
  }
  footer(high ? "Hold Enter accept  N reject  ` hide" : "Enter accept  N reject  C cancel");
}

void UiRenderer::followup(const codexdeck::TaskState& task, const String& text) {
  header("FOLLOW-UP", task.title);
  wrapped(text.length() ? text.c_str() : "Type a short prompt for this Codex task.", 8, 31, 37, 7,
          text.length() ? TEXT : DIM);
  surface().setTextColor(DIM, BG);
  surface().setCursor(180, 111);
  surface().printf("%u/240", text.length());
  footer("Enter send  Del edit  ` cancel");
}

void UiRenderer::offline(const DeckNetwork& network, const ControlDeckClient& client) {
  header("CODEX BRIDGE OFFLINE", network.connected() ? "WIFI OK" : "NO WIFI");
  wrapped(network.connected() ? "Searching the local network for the Codex bridge..."
                              : "Connecting to saved Wi-Fi. Mutating actions are disabled.",
          8, 38, 37, 4, WARN);
  surface().setTextColor(DIM, BG);
  surface().setCursor(8, 88);
  surface().print(String("Wi-Fi: ") + (network.ssid().length() ? network.ssid() : "not configured"));
  surface().setCursor(8, 101);
  surface().print(String("Bridge: ") + (client.host().length() ? client.host() : "discovering"));
  footer("R retry  W Wi-Fi  D diagnostics");
}

void UiRenderer::wifiList(const DeckNetwork& network, size_t selected) {
  header("WI-FI SETUP", "SCAN");
  if (network.networkCount() == 0) wrapped("No networks found. Press R to scan again.", 8, 38, 37, 5, WARN);
  const size_t scroll = selected >= 4 ? selected - 3 : 0;
  for (size_t visible = 0; visible < 4 && scroll + visible < network.networkCount(); ++visible) {
    const WifiNetworkEntry* entry = network.networkAt(scroll + visible);
    char detail[18];
    snprintf(detail, sizeof(detail), "%ld%s", entry->rssi, entry->secured ? "*" : "");
    row(visible, entry->ssid.c_str(), detail, scroll + visible == selected, TEXT);
  }
  footer("Enter choose  M manual  R rescan");
}

void UiRenderer::wifiSsid(const String& ssid) {
  header("MANUAL WI-FI", "SSID");
  wrapped("Enter a hidden or unlisted network name.", 8, 32, 37, 3, DIM);
  wrapped(ssid.length() ? ssid.c_str() : "Type the SSID...", 8, 72, 37, 3, ssid.length() ? TEXT : DIM);
  footer("Enter next  Del edit  ` cancel");
}

void UiRenderer::wifiPassword(const String& ssid, const String& password) {
  header("WI-FI PASSWORD", ssid.c_str());
  wrapped("Type the network password. It is stored in ESP32 Preferences and never sent to the bridge.", 8, 30, 37, 4, DIM);
  String masked;
  for (size_t index = 0; index < password.length(); ++index) masked += '*';
  wrapped(masked.length() ? masked.c_str() : "(open network or empty)", 8, 82, 37, 2, TEXT);
  footer("Enter connect  Del edit  ` cancel");
}

void UiRenderer::confirmStop(const codexdeck::TaskState& task) {
  header("STOP TASK?", "CONFIRM");
  wrapped(task.title, 8, 35, 37, 2, ACCENT);
  wrapped("This interrupts only the selected Codex turn. Other tasks keep running.", 8, 63, 37, 4, WARN);
  footer("Enter stop  ` cancel");
}

void UiRenderer::confirmClear(size_t count) {
  header("CLEAR FINISHED?", "CONFIRM");
  char message[80];
  snprintf(message, sizeof(message), "Remove %u completed, failed, or cancelled task%s from this deck?",
           count, count == 1 ? "" : "s");
  wrapped(message, 8, 34, 37, 4, WARN);
  wrapped("Active, waiting, and stale tasks stay. Codex threads are not deleted.", 8, 79, 37, 3, DIM);
  footer("Enter clear  ` cancel");
}

void UiRenderer::diagnostics(const DeckNetwork& network, const ControlDeckClient& client) {
  header("DIAGNOSTICS", "READ ONLY");
  auto& display = surface();
  display.setTextColor(TEXT, BG);
  display.setCursor(8, 27); display.printf("Wi-Fi: %s", network.connected() ? "connected" : "offline");
  display.setCursor(8, 40); display.printf("SSID: %s", network.ssid().c_str());
  display.setCursor(8, 53); display.printf("Bridge: %s:%u", client.host().c_str(), client.port());
  display.setCursor(8, 66); display.printf("Socket: %s", client.connected() ? "online" : "offline");
  display.setCursor(8, 79); display.printf("Tasks: %u", client.tasks().count());
  display.setCursor(8, 92); display.printf("Heap: %u  Render: %s", ESP.getFreeHeap(), buffered_ ? "buffered" : "direct");
  display.setCursor(8, 105); display.printf("Last bridge: %lus", client.lastSeenMs() ? (millis() - client.lastSeenMs()) / 1000 : 0);
  footer("K keymap  R retry  ` back");
}

void UiRenderer::usage(const UsageState& usage) {
  header("CODEX USAGE", usage.limitName[0] ? usage.limitName : "LIVE");
  if (!usage.received) {
    wrapped("Waiting for Codex account usage limits from the local App Server.", 8, 35, 37, 5, DIM);
  } else if (!usage.available) {
    wrapped("Codex did not return account limit windows for this session.", 8, 35, 37, 5, WARN);
  } else {
    auto& display = surface();
    const auto drawWindow = [&](int16_t y, const char* fallback, int16_t remaining, uint32_t minutes) {
      if (remaining < 0) return;
      char label[30];
      if (minutes >= 1440 && minutes % 1440 == 0)
        snprintf(label, sizeof(label), "%lu-day window", minutes / 1440);
      else if (minutes >= 60 && minutes % 60 == 0)
        snprintf(label, sizeof(label), "%lu-hour window", minutes / 60);
      else
        snprintf(label, sizeof(label), "%s", fallback);
      display.setTextColor(TEXT, BG);
      display.setCursor(8, y);
      display.printf("%s: %d%% remaining", label, remaining);
      display.drawRect(8, y + 14, 224, 12, DIM);
      display.fillRect(10, y + 16, (220 * remaining) / 100, 8,
                       remaining > 25 ? GOOD : (remaining > 10 ? WARN : BAD));
    };
    drawWindow(29, "Primary", usage.primaryRemainingPercent, usage.primaryWindowMinutes);
    drawWindow(70, "Secondary", usage.secondaryRemainingPercent, usage.secondaryWindowMinutes);
  }
  footer("Live Codex limits  ` back");
}

void UiRenderer::settings(size_t selected, const char* theme, uint8_t brightness, bool statusLightEnabled,
                          bool partyLightEnabled, bool soundEnabled) {
  header("DECK SETTINGS", "LOCAL");
  auto& display = surface();
  panel(5, 26, 230, 24, selected == 0 ? ACCENT : PANEL_ALT, PANEL, 5);
  display.setTextColor(selected == 0 ? ACCENT : TEXT, PANEL);
  display.setCursor(13, 31);
  display.print("THEME");
  display.setTextColor(selected == 0 ? ACCENT : DIM, PANEL);
  display.setCursor(13, 41);
  display.print("< ");
  display.print(theme ? theme : "NEON GRID");
  display.print(" >");
  display.fillRoundRect(188, 31, 36, 12, 4, ACCENT);
  display.fillRect(191, 34, 30, 6, CYBER_VIOLET);

  panel(5, 54, 230, 24, selected == 1 ? ACCENT : PANEL_ALT, PANEL, 5);
  display.setTextColor(selected == 1 ? ACCENT : TEXT, PANEL);
  display.setCursor(13, 59);
  display.print("DISPLAY BRIGHTNESS");
  display.setTextColor(selected == 1 ? ACCENT : DIM, PANEL);
  display.setCursor(13, 69);
  display.printf("< %3u / 255 >", brightness);
  display.drawRoundRect(145, 67, 76, 8, 3, DIM);
  display.fillRoundRect(147, 69, (72 * brightness) / 255, 4, 2, ACCENT);

  panel(3, 82, 76, 18, selected == 2 ? ACCENT : PANEL_ALT, PANEL, 4);
  panel(82, 82, 76, 18, selected == 3 ? ACCENT : PANEL_ALT, PANEL, 4);
  panel(161, 82, 76, 18, selected == 4 ? ACCENT : PANEL_ALT, PANEL, 4);
  display.setTextColor(selected == 2 ? ACCENT : TEXT, PANEL);
  display.setCursor(13, 87);
  display.print("LED");
  display.setTextColor(DIM, PANEL);
  display.setCursor(35, 87);
  display.print(statusLightEnabled ? "< ON >" : "< OFF >");
  display.setTextColor(selected == 3 ? ACCENT : TEXT, PANEL);
  display.setCursor(91, 87);
  display.print("PARTY");
  display.setTextColor(DIM, PANEL);
  display.setCursor(123, 87);
  display.print(partyLightEnabled ? "< ON >" : "< OFF >");
  display.setTextColor(selected == 4 ? ACCENT : TEXT, PANEL);
  display.setCursor(170, 87);
  display.print("SOUND");
  display.setTextColor(DIM, PANEL);
  display.setCursor(199, 87);
  display.print(soundEnabled ? "ON" : "OFF");

  panel(5, 105, 72, 12, selected == 5 ? ACCENT : PANEL_ALT, PANEL, 4);
  panel(84, 105, 72, 12, selected == 6 ? ACCENT : PANEL_ALT, PANEL, 4);
  panel(163, 105, 72, 12, selected == 7 ? ACCENT : PANEL_ALT, PANEL, 4);
  display.setTextColor(selected == 5 ? ACCENT : TEXT, PANEL);
  display.setCursor(12, 108);
  display.print("WI-FI");
  display.setTextColor(selected == 6 ? ACCENT : TEXT, PANEL);
  display.setCursor(107, 108);
  display.print("HID");
  display.setTextColor(selected == 7 ? ACCENT : TEXT, PANEL);
  display.setCursor(174, 108);
  display.print("DIAG");
  footer("Up/Down select  </> change  Enter open  ` back");
}

void UiRenderer::hidSettings(size_t selected, codexdeck::HidTransport transport, const char* status, bool bondKnown,
                             bool bonded, const char* toast) {
  header("HID SETTINGS", codexdeck::hidTransportLabel(transport));
  auto& display = surface();
  const bool ready = status && (strcmp(status, "USB READY") == 0 || strcmp(status, "BT READY") == 0);
  panel(5, 28, 112, 25, selected == 0 && transport == codexdeck::HidTransport::Usb ? ACCENT : PANEL_ALT,
        transport == codexdeck::HidTransport::Usb ? 0x106D : PANEL, 5);
  panel(123, 28, 112, 25, selected == 0 && transport == codexdeck::HidTransport::Bluetooth ? ACCENT : PANEL_ALT,
        transport == codexdeck::HidTransport::Bluetooth ? 0x106D : PANEL, 5);
  display.setTextColor(transport == codexdeck::HidTransport::Usb ? ACCENT : TEXT,
                       transport == codexdeck::HidTransport::Usb ? 0x106D : PANEL);
  display.setCursor(40, 37);
  display.print("USB");
  display.setTextColor(transport == codexdeck::HidTransport::Bluetooth ? ACCENT : TEXT,
                       transport == codexdeck::HidTransport::Bluetooth ? 0x106D : PANEL);
  display.setCursor(143, 37);
  display.print("BLUETOOTH");

  panel(5, 59, 230, 21, ready ? GOOD : WARN, PANEL, 5);
  display.setTextColor(ready ? GOOD : WARN, PANEL);
  display.setCursor(13, 66);
  display.print(status ? status : "HID WAIT");
  display.setTextColor(DIM, PANEL);
  display.setCursor(130, 66);
  display.print(!bondKnown ? "BOND UNKNOWN" : (bonded ? "BONDED" : "NOT BONDED"));

  panel(5, 87, 230, 25, selected == 1 ? WARN : PANEL_ALT, PANEL, 5);
  display.setTextColor(selected == 1 ? WARN : TEXT, PANEL);
  display.setCursor(57, 96);
  display.print("CLEAR BT PAIRING");
  footer(toast && toast[0] ? toast : "Up/Down select  </> mode  Enter  ` back");
}

void UiRenderer::keymap(const String& latest) {
  header("KEYMAP TEST", "DEVELOPMENT");
  wrapped("Press keys to inspect normalized input. Arrow coordinates and printable text appear below.", 8, 28, 37, 4, DIM);
  wrapped(latest.length() ? latest.c_str() : "Waiting for a key...", 8, 78, 37, 3, ACCENT);
  footer("` back");
}

void UiRenderer::message(const char* title, const char* body, const char* footerText) {
  header(title);
  wrapped(body, 8, 34, 37, 7, TEXT);
  footer(footerText);
}

lgfx::LovyanGFX& UiRenderer::surface() {
  return buffered_ ? static_cast<lgfx::LovyanGFX&>(*canvas_)
                   : static_cast<lgfx::LovyanGFX&>(M5Cardputer.Display);
}

void UiRenderer::backdrop() {
  auto& display = surface();
  display.fillScreen(BG);
  for (int16_t y = 22; y < display.height() - 14; y += 16) {
    display.drawFastHLine(0, y, display.width(), GRID);
  }
  for (int16_t x = 8; x < display.width(); x += 24) {
    display.drawFastVLine(x, 20, display.height() - 34, GRID);
  }
}

void UiRenderer::panel(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t border, uint16_t fill,
                       uint8_t radius) {
  auto& display = surface();
  display.drawRoundRect(x, y, width, height, radius, border);
  if (width > 2 && height > 2) display.fillRoundRect(x + 1, y + 1, width - 2, height - 2, radius - 1, fill);
}

void UiRenderer::keycap(int16_t x, int16_t y, const char* label, bool active) {
  auto& display = surface();
  const String text(label ? label : "");
  const int16_t width = max<int16_t>(12, static_cast<int16_t>(text.length() * 6 + 6));
  const uint16_t fill = active ? BG : ACCENT;
  const uint16_t color = active ? ACCENT : BG;
  display.fillRoundRect(x, y, width, 10, 3, fill);
  display.setTextColor(color, fill);
  display.setCursor(x + 3, y + 2);
  display.print(text.substring(0, 2));
}

void UiRenderer::metricChip(int16_t x, int16_t y, int16_t width, const char* label, const char* value, uint16_t color) {
  auto& display = surface();
  panel(x, y, width, 18, color, PANEL, 5);
  display.setTextColor(DIM, PANEL);
  display.setCursor(x + 6, y + 4);
  display.print(label ? label : "");
  display.setTextColor(color, PANEL);
  const String text(value ? value : "");
  display.setCursor(x + width - 7 - static_cast<int16_t>(text.length() * 6), y + 4);
  display.print(text.substring(0, 6));
}

void UiRenderer::dashboardFooter() {
  auto& display = surface();
  display.fillRect(0, display.height() - 14, display.width(), 14, FOOTER);
  const char* labels[] = {"1 NEW", "K KEYS", "S SET", "C CLR", "U LIMIT"};
  for (uint8_t index = 0; index < 5; ++index) {
    const int16_t x = 2 + index * 48;
    panel(x, display.height() - 12, 44, 10, index == 1 ? CYBER_VIOLET : GRID, PANEL, 3);
    display.setTextColor(index == 1 ? CYBER_VIOLET : DIM, PANEL);
    display.setCursor(x + 4, display.height() - 10);
    display.print(labels[index]);
  }
}

void UiRenderer::macroFooter() {
  auto& display = surface();
  display.fillRect(0, display.height() - 14, display.width(), 14, FOOTER);
  keycap(4, display.height() - 12, "<>");
  display.setTextColor(DIM, FOOTER);
  display.setCursor(20, display.height() - 10);
  display.print("PAGE");
  keycap(63, display.height() - 12, "UD");
  display.setCursor(80, display.height() - 10);
  display.print("SELECT");
  keycap(142, display.height() - 12, "E");
  display.setCursor(157, display.height() - 10);
  display.print("SEND");
  display.setTextColor(DIM, FOOTER);
  display.setCursor(205, display.height() - 10);
  display.print("`");
}

void UiRenderer::present() {
  if (buffered_) canvas_->pushSprite(0, 0);
}

void UiRenderer::logDiagnostics(const char* phase) const {
  Serial.printf("[diag] action=display_buffer status=%s phase=%s mode=%s psram=%u free_psram=%u heap=%u bytes=%u\n",
                buffered_ ? "ok" : "fallback", phase ? phase : "unknown",
                buffered_ ? "psram_canvas_16bit" : "direct", ESP.getPsramSize(), ESP.getFreePsram(),
                ESP.getFreeHeap(), buffered_ ? static_cast<unsigned>(canvas_->bufferLength()) : 0U);
}

void UiRenderer::header(const char* title, const char* indicator) {
  auto& display = surface();
  backdrop();
  display.fillRect(0, 0, display.width(), 20, HEADER);
  display.drawFastHLine(0, 19, display.width(), ACCENT);
  display.drawFastHLine(4, 21, 46, CYBER_VIOLET);
  display.drawFastHLine(display.width() - 50, 21, 46, CYBER_VIOLET);
  display.drawRect(3, 3, display.width() - 6, 14, PANEL_ALT);
  display.setTextColor(TEXT, HEADER);
  display.setCursor(6, 6);
  display.print(String(title).substring(0, 21));
  if (indicator && indicator[0]) {
    display.setTextColor(ACCENT, HEADER);
    const String value(indicator);
    display.setCursor(max(120, 236 - static_cast<int>(value.length() * 6)), 6);
    display.print(value.substring(0, 19));
  }
}

void UiRenderer::footer(const char* text) {
  auto& display = surface();
  display.fillRect(0, display.height() - 14, display.width(), 14, FOOTER);
  display.drawFastHLine(0, display.height() - 14, display.width(), GRID);
  display.setTextColor(DIM, FOOTER);
  display.setCursor(6, display.height() - 11);
  display.print(String(text).substring(0, 38));
}

void UiRenderer::row(uint8_t index, const char* label, const char* detail, bool selected, uint16_t color) {
  auto& display = surface();
  const int16_t y = 25 + index * 20;
  const uint16_t background = selected ? ACCENT : BG;
  display.fillRoundRect(4, y - 2, 232, 18, 3, background);
  display.setTextColor(selected ? BG : color, background);
  display.setCursor(8, y + 2);
  display.print(selected ? "> " : "  ");
  display.print(String(label).substring(0, 25));
  display.setTextColor(selected ? BG : DIM, background);
  const String right(detail ? detail : "");
  display.setCursor(max(160, 232 - static_cast<int>(right.length() * 6)), y + 2);
  display.print(right.substring(0, 11));
}

void UiRenderer::wrapped(const char* text, int16_t x, int16_t y, uint8_t maxChars, uint8_t maxLines, uint16_t color) {
  wrappedWindow(text, x, y, maxChars, maxLines, 0, color);
}

void UiRenderer::wrappedWindow(const char* text, int16_t x, int16_t y, uint8_t maxChars, uint8_t maxLines,
                               size_t firstLine, uint16_t color) {
  auto& display = surface();
  display.setTextColor(color, BG);
  const char* cursor = text ? text : "";
  for (size_t skipped = 0; skipped < firstLine && *cursor; ++skipped) {
    cursor = nextWrappedLine(cursor, maxChars, nullptr, 0);
  }
  for (uint8_t line = 0; line < maxLines && *cursor; ++line) {
    char output[48];
    cursor = nextWrappedLine(cursor, maxChars, output, sizeof(output));
    display.setCursor(x, y + line * 11);
    display.print(output);
  }
}
