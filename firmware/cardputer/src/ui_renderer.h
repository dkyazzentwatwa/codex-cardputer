#pragma once

#include <Arduino.h>
#include <ControlDeckCore.h>

#include "control_deck_client.h"
#include "network_manager.h"

class UiRenderer {
 public:
  void begin();
  void dashboard(const codexdeck::TaskStore& tasks, bool connected, const char* toast);
  void taskDetail(const codexdeck::TaskState& task, size_t selectedMacro);
  void macros(const ControlDeckClient& client, size_t selected, size_t page);
  void approval(const ApprovalState& approval, const char* armedDecision, bool armed, uint8_t holdPercent);
  void followup(const codexdeck::TaskState& task, const String& text);
  void offline(const DeckNetwork& network, const ControlDeckClient& client);
  void wifiList(const DeckNetwork& network, size_t selected);
  void wifiSsid(const String& ssid);
  void wifiPassword(const String& ssid, const String& password);
  void confirmStop(const codexdeck::TaskState& task);
  void diagnostics(const DeckNetwork& network, const ControlDeckClient& client);
  void keymap(const String& latest);
  void message(const char* title, const char* body, const char* footer = "` back");

 private:
  void header(const char* title, const char* indicator = "");
  void footer(const char* text);
  void row(uint8_t index, const char* label, const char* detail, bool selected, uint16_t color);
  void wrapped(const char* text, int16_t x, int16_t y, uint8_t maxChars, uint8_t maxLines, uint16_t color);
};
