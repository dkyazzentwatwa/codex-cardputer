#include "ui_renderer.h"

#include <M5Cardputer.h>

namespace {
constexpr uint16_t BG = 0x0000;
constexpr uint16_t HEADER = 0x001F;
constexpr uint16_t FOOTER = 0x0841;
constexpr uint16_t TEXT = 0xFFFF;
constexpr uint16_t DIM = 0x8410;
constexpr uint16_t ACCENT = 0x07FF;
constexpr uint16_t GOOD = 0x07E0;
constexpr uint16_t WARN = 0xFFE0;
constexpr uint16_t BAD = 0xF800;

uint16_t statusColor(codexdeck::TaskStatus status) {
  if (status == codexdeck::TaskStatus::Completed) return GOOD;
  if (status == codexdeck::TaskStatus::Failed || status == codexdeck::TaskStatus::Cancelled) return BAD;
  if (status == codexdeck::TaskStatus::WaitingApproval || status == codexdeck::TaskStatus::WaitingInput) return WARN;
  if (status == codexdeck::TaskStatus::Stale) return DIM;
  return TEXT;
}
}  // namespace

void UiRenderer::begin() {
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextDatum(top_left);
  M5Cardputer.Display.setTextWrap(false);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setBrightness(180);
  M5Cardputer.Display.fillScreen(BG);
}

void UiRenderer::dashboard(const codexdeck::TaskStore& tasks, bool connected, const char* toast) {
  char indicator[24];
  size_t running = 0;
  size_t waiting = 0;
  for (size_t index = 0; index < tasks.count(); ++index) {
    const codexdeck::TaskState* task = tasks.at(index);
    if (task->status == codexdeck::TaskStatus::Running) ++running;
    if (task->requiresAttention) ++waiting;
  }
  snprintf(indicator, sizeof(indicator), "%uR %uW %s", running, waiting, connected ? "ON" : "OFF");
  header("CODEX", indicator);
  if (tasks.count() == 0) {
    wrapped(connected ? "No tasks yet. Open NEW to launch a saved workflow or skill."
                      : "Bridge offline. Recent tasks will appear stale until reconnection.",
            8, 34, 37, 6, connected ? TEXT : WARN);
  } else {
    const size_t selected = tasks.selectedIndex();
    const size_t scroll = selected >= 4 ? selected - 3 : 0;
    for (size_t visible = 0; visible < 4 && scroll + visible < tasks.count(); ++visible) {
      const codexdeck::TaskState* task = tasks.at(scroll + visible);
      row(visible, task->title, codexdeck::statusLabel(task->status), scroll + visible == selected, statusColor(task->status));
    }
  }
  if (toast && toast[0]) {
    M5Cardputer.Display.setTextColor(ACCENT, BG);
    M5Cardputer.Display.setCursor(6, 106);
    M5Cardputer.Display.print(String(toast).substring(0, 38));
  }
  footer("1 NEW  Tab attention  Enter open");
}

void UiRenderer::taskDetail(const codexdeck::TaskState& task, size_t selectedMacro) {
  header(task.title, codexdeck::statusLabel(task.status));
  M5Cardputer.Display.setTextColor(statusColor(task.status), BG);
  M5Cardputer.Display.setCursor(8, 28);
  M5Cardputer.Display.printf("%s  %02lu:%02lu", codexdeck::statusLabel(task.status),
                            task.elapsedSeconds / 60, task.elapsedSeconds % 60);
  wrapped(task.summary, 8, 45, 37, 4, TEXT);
  if (task.status == codexdeck::TaskStatus::WaitingInput) {
    wrapped("Answer this request in the bridge terminal.", 8, 92, 37, 2, WARN);
  } else if (task.detail[0]) {
    wrapped(task.detail, 8, 89, 37, 1, DIM);
  }
  if (task.macroCount) {
    const codexdeck::MacroState& macro = task.macros[min(selectedMacro, task.macroCount - 1)];
    M5Cardputer.Display.setTextColor(ACCENT, BG);
    M5Cardputer.Display.setCursor(8, 103);
    M5Cardputer.Display.printf("> %s", macro.shortLabel);
  }
  footer("Up/Down action  Enter run  ` back");
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
    M5Cardputer.Display.fillRect(4, y, 232, 11, bg);
    M5Cardputer.Display.setTextColor(active ? BG : (macro->enabled ? TEXT : DIM), bg);
    M5Cardputer.Display.setCursor(7, y + 2);
    M5Cardputer.Display.printf("%u %s", item + 1, macro->label);
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
    M5Cardputer.Display.drawRect(10, 101, 220, 10, DIM);
    M5Cardputer.Display.fillRect(11, 102, (218 * holdPercent) / 100, 8, BAD);
  } else if (armed) {
    M5Cardputer.Display.setTextColor(WARN, BG);
    M5Cardputer.Display.setCursor(8, 102);
    M5Cardputer.Display.printf("PRESS %s AGAIN", armedDecision);
  }
  footer(high ? "Hold Enter accept  N reject  ` hide" : "Enter accept  N reject  C cancel");
}

void UiRenderer::followup(const codexdeck::TaskState& task, const String& text) {
  header("FOLLOW-UP", task.title);
  wrapped(text.length() ? text.c_str() : "Type a short prompt for this Codex task.", 8, 31, 37, 7,
          text.length() ? TEXT : DIM);
  M5Cardputer.Display.setTextColor(DIM, BG);
  M5Cardputer.Display.setCursor(180, 111);
  M5Cardputer.Display.printf("%u/240", text.length());
  footer("Enter send  Del edit  ` cancel");
}

void UiRenderer::offline(const DeckNetwork& network, const ControlDeckClient& client) {
  header("CODEX BRIDGE OFFLINE", network.connected() ? "WIFI OK" : "NO WIFI");
  wrapped(network.connected() ? "Searching the local network for the Codex bridge..."
                              : "Connecting to saved Wi-Fi. Mutating actions are disabled.",
          8, 38, 37, 4, WARN);
  M5Cardputer.Display.setTextColor(DIM, BG);
  M5Cardputer.Display.setCursor(8, 88);
  M5Cardputer.Display.print(String("Wi-Fi: ") + (network.ssid().length() ? network.ssid() : "not configured"));
  M5Cardputer.Display.setCursor(8, 101);
  M5Cardputer.Display.print(String("Bridge: ") + (client.host().length() ? client.host() : "discovering"));
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

void UiRenderer::diagnostics(const DeckNetwork& network, const ControlDeckClient& client) {
  header("DIAGNOSTICS", "READ ONLY");
  auto& display = M5Cardputer.Display;
  display.setTextColor(TEXT, BG);
  display.setCursor(8, 27); display.printf("Wi-Fi: %s", network.connected() ? "connected" : "offline");
  display.setCursor(8, 40); display.printf("SSID: %s", network.ssid().c_str());
  display.setCursor(8, 53); display.printf("Bridge: %s:%u", client.host().c_str(), client.port());
  display.setCursor(8, 66); display.printf("Socket: %s", client.connected() ? "online" : "offline");
  display.setCursor(8, 79); display.printf("Tasks: %u", client.tasks().count());
  display.setCursor(8, 92); display.printf("Heap: %u", ESP.getFreeHeap());
  display.setCursor(8, 105); display.printf("Last bridge: %lus", client.lastSeenMs() ? (millis() - client.lastSeenMs()) / 1000 : 0);
  footer("K keymap  R retry  ` back");
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

void UiRenderer::header(const char* title, const char* indicator) {
  auto& display = M5Cardputer.Display;
  display.fillScreen(BG);
  display.fillRect(0, 0, display.width(), 20, HEADER);
  display.setTextColor(TEXT, HEADER);
  display.setCursor(6, 6);
  display.print(String(title).substring(0, 25));
  if (indicator && indicator[0]) {
    display.setTextColor(ACCENT, HEADER);
    const String value(indicator);
    display.setCursor(max(120, 236 - static_cast<int>(value.length() * 6)), 6);
    display.print(value.substring(0, 19));
  }
}

void UiRenderer::footer(const char* text) {
  auto& display = M5Cardputer.Display;
  display.fillRect(0, display.height() - 14, display.width(), 14, FOOTER);
  display.setTextColor(DIM, FOOTER);
  display.setCursor(6, display.height() - 11);
  display.print(String(text).substring(0, 38));
}

void UiRenderer::row(uint8_t index, const char* label, const char* detail, bool selected, uint16_t color) {
  auto& display = M5Cardputer.Display;
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
  auto& display = M5Cardputer.Display;
  display.setTextColor(color, BG);
  const char* cursor = text ? text : "";
  for (uint8_t line = 0; line < maxLines && *cursor; ++line) {
    char output[48];
    uint8_t length = 0;
    const char* start = cursor;
    const char* lastSpace = nullptr;
    while (*cursor && *cursor != '\n' && length < maxChars) {
      if (*cursor == ' ') lastSpace = cursor;
      ++cursor;
      ++length;
    }
    if (*cursor && *cursor != '\n' && lastSpace && lastSpace > start) {
      length = static_cast<uint8_t>(lastSpace - start);
      cursor = lastSpace + 1;
    } else if (*cursor == '\n') {
      ++cursor;
    }
    if (length >= sizeof(output)) length = sizeof(output) - 1;
    memcpy(output, start, length);
    output[length] = '\0';
    display.setCursor(x, y + line * 11);
    display.print(output);
  }
}
