#include "ui_renderer.h"

#include <Adafruit_XCA9554.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <Wire.h>

namespace {
constexpr int LCD_SCLK = 11;
constexpr int LCD_CS = 12;
constexpr int LCD_SDIO0 = 4;
constexpr int LCD_SDIO1 = 5;
constexpr int LCD_SDIO2 = 6;
constexpr int LCD_SDIO3 = 7;
constexpr int LCD_RST = -1;
constexpr uint8_t EXPANDER_ADDRESS = 0x20;

Adafruit_XCA9554 expander;
Arduino_ESP32QSPI* qspi = nullptr;
Arduino_SH8601* sh8601 = nullptr;
Arduino_Canvas* canvas = nullptr;

AmoledRenderer::Palette paletteFor(codexdeck::DeckTheme theme) {
  switch (theme) {
    case codexdeck::DeckTheme::Terminal: return {0x0000, 0x0200, 0x0320, 0xCFE0, 0x5BE0, 0x07E0, 0x03C0, 0x07E0, 0xFD20, 0xF800};
    case codexdeck::DeckTheme::AmberCrt: return {0x0000, 0x2940, 0x4200, 0xFFE0, 0xCBE0, 0xFD20, 0xCB20, 0xBFE0, 0xFFE0, 0xF800};
    case codexdeck::DeckTheme::Synthwave: return {0x1008, 0x2811, 0x4012, 0xFFFF, 0xC79F, 0xF81F, 0x07FF, 0x07E0, 0xFFE0, 0xF800};
    case codexdeck::DeckTheme::Ice: return {0x0012, 0x18D6, 0x18D6, 0xFFFF, 0xBDF7, 0x7DFF, 0x3D9F, 0x07E0, 0xFFE0, 0xF800};
    case codexdeck::DeckTheme::NeonGrid:
    default: return {0x0009, 0x0842, 0x1084, 0xFFFF, 0x8410, 0x07FF, 0xA19F, 0x07E0, 0xFFE0, 0xF800};
  }
}

const char* shortStatus(const codexdeck::TaskState& task) { return codexdeck::statusLabel(task.status); }

const char* wifiStatusLabel() {
  switch (WiFi.status()) {
    case WL_CONNECTED: return "CONNECTED";
    case WL_NO_SSID_AVAIL: return "SSID NOT FOUND";
    case WL_CONNECT_FAILED: return "AUTH FAILED";
    case WL_CONNECTION_LOST: return "LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    case WL_IDLE_STATUS: return "CONNECTING";
    default: return "STARTING";
  }
}
}  // namespace

bool AmoledRenderer::begin() {
  Wire.begin(15, 14);
  const bool expanderReady = expander.begin(EXPANDER_ADDRESS, &Wire);
  if (!expanderReady) {
    Serial.println("[diag] action=amoled_power status=fail expander=missing");
  } else {
    // The panel rails are latched through the XCA9554. Reset all three before
    // enabling them together, matching Waveshare's required power sequence.
    for (uint8_t pin = 0; pin < 3; ++pin) {
      expander.pinMode(pin, OUTPUT);
      expander.digitalWrite(pin, LOW);
    }
    delay(20);
    for (uint8_t pin = 0; pin < 3; ++pin) expander.digitalWrite(pin, HIGH);
    delay(120);
  }
  qspi = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
  sh8601 = new Arduino_SH8601(qspi, LCD_RST, 0, 368, 448);
  if (!sh8601 || !sh8601->begin()) {
    Serial.println("[diag] action=display_init status=fail controller=SH8601");
    return false;
  }
  sh8601->setRotation(0);
  sh8601->setTextWrap(false);
  sh8601->setBrightness(brightness_);
  // A brief high-contrast checkpoint drawn straight to the panel makes power/
  // QSPI failures obvious before the PSRAM canvas, bridge, Wi-Fi, or touch
  // startup can obscure the root cause.
  sh8601->fillScreen(0x001F);
  sh8601->fillRect(0, 0, width(), 56, 0x07E0);
  sh8601->setTextColor(0xFFFF, 0x07E0);
  sh8601->setTextSize(2);
  sh8601->setCursor(18, 18);
  sh8601->print("CODEXDECK AMOLED");
  sh8601->setTextSize(1);
  delay(700);

  // Compose every frame off-screen in an 8 MB-PSRAM framebuffer and push it in
  // one QSPI burst (present()). This removes the fillScreen-then-repaint flash
  // that made every touch flicker. Falls back to direct rendering if the
  // framebuffer cannot be allocated so the panel is never left blank.
  const uint32_t psramBefore = ESP.getFreePsram();
  canvas = new Arduino_Canvas(368, 448, sh8601);
  usingCanvas_ = canvas && canvas->begin(GFX_SKIP_OUTPUT_BEGIN) && canvas->getFramebuffer() != nullptr;
  display_ = usingCanvas_ ? static_cast<Arduino_GFX*>(canvas) : static_cast<Arduino_GFX*>(sh8601);
  display_->setTextWrap(false);
  Serial.printf("[diag] action=display_init status=ok controller=SH8601 width=%u height=%u canvas=%s "
                "psram_total=%u psram_free_before=%u psram_free_after=%u heap=%u\n",
                width(), height(), usingCanvas_ ? "psram" : "direct-fallback", ESP.getPsramSize(),
                psramBefore, ESP.getFreePsram(), ESP.getFreeHeap());
  display_->fillScreen(palette_.bg);
  present();
  return true;
}

void AmoledRenderer::theme(codexdeck::DeckTheme value) { theme_ = value; palette_ = paletteFor(value); }

void AmoledRenderer::brightness(uint8_t value) {
  brightness_ = value;
  if (sh8601) sh8601->setBrightness(value);
}

void AmoledRenderer::background() {
  if (!display_) return;
  display_->fillScreen(palette_.bg);
  for (int y = 42; y < 420; y += 28) display_->drawFastHLine(0, y, width(), palette_.panelAlt);
  for (int x = 16; x < width(); x += 32) display_->drawFastVLine(x, 42, 370, palette_.panelAlt);
}

void AmoledRenderer::header(const char* title, const char* status) {
  background();
  display_->fillRect(0, 0, width(), 58, palette_.panel);
  display_->drawFastHLine(0, 57, width(), palette_.accent);
  label(title, 18, 13, palette_.text, 3);
  if (status && status[0]) label(status, 270, 18, palette_.accent, 2);
  dirty_ = true;
}

void AmoledRenderer::footer(const char* text) {
  display_->fillRect(0, 410, width(), 38, palette_.panel);
  display_->drawFastHLine(0, 410, width(), palette_.panelAlt);
  label(text, 18, 421, palette_.dim, 1);
}

void AmoledRenderer::panel(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border, uint16_t fill, uint8_t radius) {
  display_->fillRoundRect(x, y, w, h, radius, fill);
  display_->drawRoundRect(x, y, w, h, radius, border);
}

void AmoledRenderer::label(const char* value, int16_t x, int16_t y, uint16_t color, uint8_t size) {
  display_->setTextSize(size);
  display_->setTextColor(color, palette_.bg);
  display_->setCursor(x, y);
  display_->print(value ? value : "");
  display_->setTextSize(1);
}

void AmoledRenderer::wrapped(const char* value, int16_t x, int16_t y, uint8_t chars, uint8_t lines, uint16_t color, uint8_t size) {
  String text(value ? value : "");
  size_t offset = 0;
  const uint8_t charWidth = size == 1 ? 6 : 12;
  for (uint8_t line = 0; line < lines && offset < text.length(); ++line) {
    const size_t maxLength = min<size_t>(chars, text.length() - offset);
    size_t length = maxLength;
    if (length == chars) {
      const int space = text.substring(offset, offset + length).lastIndexOf(' ');
      if (space > 0) length = static_cast<size_t>(space);
    }
    String part = text.substring(offset, offset + length);
    label(part.c_str(), x, y + line * (size == 1 ? 15 : 25), color, size);
    offset += length;
    while (offset < text.length() && text[offset] == ' ') ++offset;
    (void)charWidth;
  }
}

void AmoledRenderer::button(int16_t x, int16_t y, int16_t w, int16_t h, const char* value, uint16_t color, bool selected) {
  panel(x, y, w, h, color, selected ? color : palette_.panel, 10);
  display_->setTextColor(selected ? palette_.bg : color, selected ? color : palette_.panel);
  display_->setTextSize(2);
  display_->setCursor(x + 14, y + (h - 16) / 2);
  display_->print(value ? value : "");
  display_->setTextSize(1);
}

void AmoledRenderer::dashboard(const codexdeck::TaskStore& tasks, bool bridge, bool hidReady, const char* toast) {
  header("CODEX DECK", bridge ? (hidReady ? "READY" : "HID") : "OFFLINE");
  panel(16, 72, 106, 48, palette_.good, palette_.panel, 12);
  panel(131, 72, 106, 48, palette_.warn, palette_.panel, 12);
  panel(246, 72, 106, 48, palette_.accent, palette_.panel, 12);
  label("ACTIVE", 29, 82, palette_.dim, 1); label(String(tasks.count()).c_str(), 29, 97, palette_.good, 3);
  label("ATTN", 144, 82, palette_.dim, 1); label("LIVE", 144, 97, palette_.warn, 2);
  label("HID", 259, 82, palette_.dim, 1); label(hidReady ? "READY" : "PLUG USB", 259, 97, palette_.accent, 2);
  if (tasks.count() == 0) {
    panel(16, 140, 336, 130, bridge ? palette_.accent : palette_.warn, palette_.panel, 14);
    label(bridge ? "NO ACTIVE TASKS" : "BRIDGE OFFLINE", 57, 175, bridge ? palette_.accent : palette_.warn, 3);
    wrapped(bridge ? "Launch a workflow from Codex to see it here." : "Check Wi-Fi and bridge discovery in Settings.", 44, 225, 28, 2, palette_.dim, 2);
  } else {
    for (size_t index = 0; index < min<size_t>(3, tasks.count()); ++index) {
      const codexdeck::TaskState* task = tasks.at(index);
      const int y = 140 + index * 82;
      panel(16, y, 336, 68, index == tasks.selectedIndex() ? palette_.accent : palette_.panelAlt,
            index == tasks.selectedIndex() ? palette_.panel : palette_.panel, 12);
      display_->fillCircle(36, y + 24, 7, task->requiresAttention ? palette_.warn : palette_.good);
      wrapped(task->title, 56, y + 13, 26, 2, palette_.text, 2);
      label(shortStatus(*task), 272, y + 44, task->requiresAttention ? palette_.warn : palette_.dim, 1);
    }
  }
  if (toast && toast[0]) { panel(34, 324, 300, 40, palette_.accent, palette_.panel, 10); label(toast, 49, 337, palette_.accent, 1); }
  button(16, 370, 100, 30, "NEW", palette_.accent);
  button(124, 370, 100, 30, "KEYS", palette_.violet);
  button(232, 370, 120, 30, "SETTINGS", palette_.dim);
  footer("Tap task  Swipe list  Tap actions");
}

void AmoledRenderer::detail(const codexdeck::TaskState& task, size_t scrollLine) {
  header(task.title, codexdeck::statusLabel(task.status));
  label(codexdeck::statusLabel(task.status), 20, 78, task.requiresAttention ? palette_.warn : palette_.good, 2);
  wrapped(task.detail[0] ? task.detail : task.summary, 20, 118, 28, 8, palette_.text, 2);
  label(String("Scroll ") + String(scrollLine + 1), 20, 345, palette_.dim, 1);
  button(16, 370, 100, 30, "BACK", palette_.dim);
  button(124, 370, 100, 30, "FOLLOW", palette_.accent);
  button(232, 370, 120, 30, "STOP", palette_.bad);
  footer("Swipe text  Tap action");
}

void AmoledRenderer::keyboard(const char* hidStatus, size_t selected, size_t page, const char* toast) {
  char title[32];
  snprintf(title, sizeof(title), "MACROS  %u/3", page + 1);
  header(title, hidStatus);
  const size_t start = codexdeck::keyboardShortcutPageStart(page);
  const size_t count = codexdeck::keyboardShortcutPageItemCount(page);
  label(codexdeck::keyboardShortcutPageLabel(page), 20, 75, palette_.violet, 2);
  for (size_t index = 0; index < count; ++index) {
    const size_t absolute = start + index;
    const auto* shortcut = codexdeck::keyboardShortcutAt(absolute);
    const int col = index % 2;
    const int row = index / 2;
    button(16 + col * 176, 112 + row * 58, 160, 48, shortcut->label, palette_.accent, absolute == selected);
    label(String(index + 1).c_str(), 25 + col * 176, 119 + row * 58, palette_.dim, 1);
  }
  if (toast && toast[0]) { panel(30, 330, 308, 34, palette_.good, palette_.panel, 10); label(toast, 45, 342, palette_.good, 1); }
  button(16, 370, 100, 30, "HOME", palette_.dim);
  button(124, 370, 100, 30, "PREV", palette_.violet);
  button(232, 370, 120, 30, "NEXT", palette_.violet);
  footer("Tap shortcut  Swipe pages");
}

void AmoledRenderer::macros(const ControlDeckClient& client, size_t selected) {
  header("WORKFLOWS", "TOUCH");
  if (client.globalMacroCount() == 0) {
    wrapped("No workflows or skills are configured in the Codex bridge.", 20, 120, 27, 3, palette_.dim, 2);
  }
  for (size_t index = 0; index < min<size_t>(6, client.globalMacroCount()); ++index) {
    const auto* macro = client.globalMacro(index);
    button(20, 82 + index * 48, 328, 38, macro->label, macro->enabled ? palette_.accent : palette_.dim,
           index == selected);
  }
  button(20, 370, 150, 30, "HOME", palette_.dim);
  footer("Tap workflow to launch");
}

void AmoledRenderer::approval(const ApprovalState& approval) {
  header("APPROVAL", approval.risk);
  label(approval.title, 20, 80, palette_.warn, 2);
  wrapped(approval.summary, 20, 125, 28, 6, palette_.text, 2);
  button(16, 330, 100, 46, "ACCEPT", palette_.good);
  button(134, 330, 100, 46, "DECLINE", palette_.bad);
  button(252, 330, 100, 46, "CANCEL", palette_.warn);
  footer("Tap a decision");
}

void AmoledRenderer::textEntry(const char* title, const String& value, bool masked, const char* hint) {
  header(title, "INPUT");
  label(value.length() ? (masked ? String('*').substring(0, 1).c_str() : value.c_str()) : hint, 20, 82, value.length() ? palette_.text : palette_.dim, 2);
  const char* rows[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  for (uint8_t row = 0; row < 4; ++row) {
    const size_t count = strlen(rows[row]);
    const int keyW = 32;
    const int startX = (368 - static_cast<int>(count) * keyW) / 2;
    for (size_t col = 0; col < count; ++col) {
      char key[2] = {rows[row][col], 0};
      button(startX + col * keyW, 155 + row * 48, 28, 38, key, palette_.accent);
    }
  }
  button(16, 355, 96, 36, "DEL", palette_.bad);
  button(120, 355, 96, 36, "SPACE", palette_.dim);
  button(224, 355, 128, 36, "ENTER", palette_.good);
  footer("Tap keys  Enter submit  Back home");
}

void AmoledRenderer::confirm(const char* title, const char* body) {
  header(title, "CONFIRM");
  panel(20, 90, 328, 160, palette_.warn, palette_.panel, 14);
  wrapped(body, 40, 125, 24, 4, palette_.text, 2);
  button(30, 290, 140, 48, "CANCEL", palette_.dim);
  button(198, 290, 140, 48, "CONFIRM", palette_.warn);
  footer("Tap an option");
}

void AmoledRenderer::usage(const UsageState& usage) {
  header("CODEX USAGE", usage.received ? "LIVE" : "WAIT");
  if (!usage.available) wrapped("Waiting for account usage limits from the local App Server.", 20, 110, 27, 4, palette_.dim, 2);
  else {
    label(String("Primary: ") + String(usage.primaryRemainingPercent) + "% remaining", 20, 105, palette_.text, 2);
    display_->drawRoundRect(20, 145, 328, 24, 8, palette_.dim);
    display_->fillRoundRect(24, 149, (320 * max<int16_t>(0, usage.primaryRemainingPercent)) / 100, 16, 6, palette_.good);
    label(String("Secondary: ") + String(usage.secondaryRemainingPercent) + "% remaining", 20, 215, palette_.text, 2);
    display_->drawRoundRect(20, 255, 328, 24, 8, palette_.dim);
    display_->fillRoundRect(24, 259, (320 * max<int16_t>(0, usage.secondaryRemainingPercent)) / 100, 16, 6, palette_.warn);
  }
  button(16, 370, 140, 30, "HOME", palette_.dim);
  footer("Live Codex limits");
}

void AmoledRenderer::settings(size_t selected, codexdeck::DeckTheme current, uint8_t brightness, bool hidReady, bool touchReady) {
  header("SETTINGS", "LOCAL");
  const char* rows[] = {"THEME", "BRIGHTNESS", "HID STATUS", "TOUCH STATUS"};
  const String values[] = {codexdeck::deckThemeLabel(current), String(brightness), hidReady ? "READY" : "PLUG USB", touchReady ? "READY" : "MISSING"};
  for (size_t index = 0; index < 4; ++index) {
    const int y = 84 + index * 62;
    panel(20, y, 328, 48, index == selected ? palette_.accent : palette_.panelAlt, palette_.panel, 10);
    label(rows[index], 38, y + 10, index == selected ? palette_.accent : palette_.text, 2);
    label(values[index].c_str(), 220, y + 13, index == selected ? palette_.accent : palette_.dim, 1);
  }
  button(10, 372, 84, 30, "WIFI", palette_.accent);
  button(100, 372, 84, 30, "DIAG", palette_.violet);
  button(190, 372, 78, 30, "CAL", palette_.good);
  button(274, 372, 84, 30, "HOME", palette_.dim);
  footer("Tap row  DIAG shows Wi-Fi state  CAL = touch");
}

void AmoledRenderer::wifiList(const DeckNetwork& network, size_t selected) {
  header("WI-FI", "SCAN");
  if (network.networkCount() == 0) wrapped("No networks found. Tap rescan or use a saved bridge.", 20, 110, 27, 3, palette_.warn, 2);
  for (size_t index = 0; index < min<size_t>(6, network.networkCount()); ++index) {
    const auto* entry = network.networkAt(index);
    panel(20, 82 + index * 45, 328, 36, index == selected ? palette_.accent : palette_.panelAlt, palette_.panel, 8);
    label(entry->ssid.c_str(), 34, 91 + index * 45, index == selected ? palette_.accent : palette_.text, 1);
    label(entry->secured ? "LOCK" : "OPEN", 275, 91 + index * 45, palette_.dim, 1);
  }
  button(20, 370, 150, 30, "RESCAN", palette_.accent);
  button(188, 370, 160, 30, "HOME", palette_.dim);
  footer("Tap network  Tap rescan");
}

void AmoledRenderer::offline(const DeckNetwork& network, const ControlDeckClient& client) {
  header("BRIDGE OFFLINE", network.connected() ? "DISCOVERING" : "NO WIFI");
  wrapped(network.connected() ? "Searching the local network for CodexDeck." : "Connect Wi-Fi from Settings.", 20, 110, 27, 4, palette_.warn, 2);
  label(String("SSID: ") + (network.ssid().length() ? network.ssid() : "not configured"), 20, 235, palette_.dim, 2);
  label(String("Wi-Fi: ") + wifiStatusLabel(), 20, 270, palette_.warn, 2);
  label(String("Bridge: ") + (client.host().length() ? client.host() : "discovering"), 20, 305, palette_.dim, 1);
  button(12, 350, 108, 38, "KEYS", palette_.accent);
  button(130, 350, 108, 38, "WI-FI", palette_.warn);
  button(248, 350, 108, 38, "SET", palette_.dim);
  footer("Keys work offline  Wi-Fi uses local settings");
}

void AmoledRenderer::calibration(const TouchSample& sample, const TouchOrientation& orient, bool touchReady) {
  header("TOUCH CAL", touchReady ? "LIVE" : "NO TOUCH");
  // Fixed corner + center targets; the mapped marker should land on the one
  // under the finger when the orientation is correct.
  const int16_t targets[5][2] = {{28, 96}, {340, 96}, {184, 224}, {28, 352}, {340, 352}};
  for (const auto& t : targets) {
    display_->drawCircle(t[0], t[1], 14, palette_.dim);
    display_->fillCircle(t[0], t[1], 2, palette_.dim);
  }
  char line[48];
  snprintf(line, sizeof(line), "raw  %3u,%3u", sample.rawX, sample.rawY);
  label(line, 20, 108, palette_.accent, 2);
  snprintf(line, sizeof(line), "map  %3u,%3u", sample.x, sample.y);
  label(line, 20, 136, palette_.good, 2);
  snprintf(line, sizeof(line), "swapXY=%d  flipX=%d  flipY=%d", orient.swapXY, orient.flipX, orient.flipY);
  label(line, 20, 300, palette_.violet, 1);
  label(sample.pressed ? "FINGER DOWN" : "touch a corner to test", 20, 315,
        sample.pressed ? palette_.good : palette_.dim, 1);
  if (sample.pressed) {
    const int16_t mx = sample.x;
    const int16_t my = sample.y;
    display_->drawFastHLine(mx - 18, my, 36, palette_.bad);
    display_->drawFastVLine(mx, my - 18, 36, palette_.bad);
    display_->drawCircle(mx, my, 11, palette_.warn);
  }
  button(12, 332, 80, 34, "SWAP", palette_.accent, orient.swapXY);
  button(98, 332, 80, 34, "FLIPX", palette_.accent, orient.flipX);
  button(184, 332, 80, 34, "FLIPY", palette_.accent, orient.flipY);
  button(270, 332, 86, 34, "SAVE", palette_.good);
  button(12, 374, 120, 32, "HOME", palette_.dim);
  button(140, 374, 120, 32, "RESET", palette_.warn);
  footer("Toggle until marker tracks finger, then SAVE");
}

void AmoledRenderer::diagnostics(const DiagInfo& info) {
  header("DIAGNOSTICS", info.wifiConnected ? "WIFI OK" : "WIFI DOWN");
  int16_t y = 74;
  auto row = [&](const char* key, const String& value, uint16_t color) {
    label(key, 20, y, palette_.dim, 1);
    label(value, 150, y, color, 1);
    y += 20;
  };
  row("secrets", info.secretsPresent ? "included" : "MISSING",
      info.secretsPresent ? palette_.good : palette_.bad);
  row("ssid cfg", info.ssidLen ? (String("len ") + (int)info.ssidLen) : String("none"),
      info.ssidLen ? palette_.good : palette_.bad);
  row("ssid scan",
      info.ssidSeen ? (String("seen rssi ") + (int)info.rssi + " ch" + (int)info.channel)
                    : String("NOT FOUND"),
      info.ssidSeen ? palette_.good : palette_.warn);
  row("ssid auth", info.ssidSeen ? (info.authOpen ? "open" : "secured") : "-", palette_.text);
  row("wifi", info.wifiStatus, info.wifiConnected ? palette_.good : palette_.warn);
  row("ip", info.wifiConnected ? info.ip : String("-"), palette_.text);
  row("bridge",
      info.bridge.length() ? (info.bridge + (info.bridgeConnected ? " connected" : " connecting"))
                           : String("discovering"),
      info.bridge.length() && info.bridgeConnected ? palette_.good : palette_.warn);
  row("touch",
      info.touchReady ? (String("FT3168 id 0x") + String((unsigned)info.touchId, HEX)) : String("MISSING"),
      info.touchReady ? palette_.good : palette_.bad);
  row("display", info.canvasPsram ? "SH8601 canvas/psram" : "SH8601 direct", palette_.good);
  row("heap", String(info.freeHeap / 1024) + "k free", palette_.text);
  row("psram", String(info.freePsram / 1024) + "k free", palette_.text);
  row("uptime", String(info.uptimeS) + "s", palette_.text);
  button(12, 372, 130, 32, "HOME", palette_.dim);
  button(150, 372, 130, 32, "TOUCH CAL", palette_.accent);
  footer("Wi-Fi password is never shown or logged");
}

void AmoledRenderer::toastMessage(const char* title, const char* body) {
  header(title, "NOTICE");
  wrapped(body, 20, 110, 27, 6, palette_.text, 2);
  button(20, 350, 150, 38, "HOME", palette_.accent);
  footer("Tap home");
}

void AmoledRenderer::present() {
  if (usingCanvas_ && canvas) canvas->flush();
  dirty_ = false;
}
