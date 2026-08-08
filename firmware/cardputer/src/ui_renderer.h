#pragma once

#include <Arduino.h>
#include <ControlDeckCore.h>
#include <M5Cardputer.h>

#include "control_deck_client.h"
#include "network_manager.h"

class UiRenderer {
 public:
  void begin();
  void setTheme(codexdeck::DeckTheme theme);
  void boot(const char* status, uint8_t progress);
  void dashboard(const codexdeck::TaskStore& tasks, bool connected, bool hidReady, const char* toast);
  void keyboard(const char* hidStatus, size_t selected, size_t page, const char* toast);
  void taskDetail(const codexdeck::TaskState& task, size_t selectedMacro, size_t scrollLine);
  size_t taskLineCount(const codexdeck::TaskState& task) const;
  void macros(const ControlDeckClient& client, size_t selected, size_t page);
  void approval(const ApprovalState& approval, const char* armedDecision, bool armed, uint8_t holdPercent);
  void followup(const codexdeck::TaskState& task, const String& text);
  void offline(const DeckNetwork& network, const ControlDeckClient& client);
  void wifiList(const DeckNetwork& network, size_t selected);
  void wifiSsid(const String& ssid);
  void wifiPassword(const String& ssid, const String& password);
  void confirmStop(const codexdeck::TaskState& task);
  void confirmClear(size_t count);
  void diagnostics(const DeckNetwork& network, const ControlDeckClient& client);
  void usage(const UsageState& usage);
  void settings(size_t selected, const char* theme, uint8_t brightness, bool statusLightEnabled, bool partyLightEnabled,
                bool soundEnabled);
  void hidSettings(size_t selected, codexdeck::HidTransport transport, const char* status, bool bondKnown,
                   bool bonded, const char* toast);
  void keymap(const String& latest);
  void message(const char* title, const char* body, const char* footer = "` back");
  void present();
  void logDiagnostics(const char* phase) const;
  bool buffered() const { return buffered_; }

 private:
  lgfx::LovyanGFX& surface();
  void backdrop();
  void panel(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t border, uint16_t fill,
             uint8_t radius = 4);
  void keycap(int16_t x, int16_t y, const char* label, bool active = false);
  void metricChip(int16_t x, int16_t y, int16_t width, const char* label, const char* value, uint16_t color);
  void dashboardFooter();
  void macroFooter();
  void header(const char* title, const char* indicator = "");
  void footer(const char* text);
  void row(uint8_t index, const char* label, const char* detail, bool selected, uint16_t color);
  void wrapped(const char* text, int16_t x, int16_t y, uint8_t maxChars, uint8_t maxLines, uint16_t color);
  void wrappedWindow(const char* text, int16_t x, int16_t y, uint8_t maxChars, uint8_t maxLines,
                     size_t firstLine, uint16_t color);

  M5Canvas* canvas_ = nullptr;
  bool buffered_ = false;
};
