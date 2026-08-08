#pragma once

#include <Arduino.h>
#include <ControlDeckCore.h>
#include <Arduino_GFX.h>

#include "shared_runtime.h"
#include "touch_input.h"

// Snapshot of boot/runtime diagnostics rendered on the on-screen DIAG page so
// the panel itself can prove Wi-Fi/touch/display state when USB serial is
// unavailable. Never carries the Wi-Fi password.
struct DiagInfo {
  bool secretsPresent = false;
  uint8_t ssidLen = 0;
  bool ssidSeen = false;
  int32_t rssi = 0;
  uint8_t channel = 0;
  bool authOpen = false;
  const char* wifiStatus = "";
  bool wifiConnected = false;
  String ip;
  bool touchReady = false;
  uint32_t touchId = 0;
  bool bridgeConnected = false;
  bool canvasPsram = false;
  uint32_t freeHeap = 0;
  uint32_t freePsram = 0;
  uint32_t uptimeS = 0;
  String bridge;
};

class AmoledRenderer {
 public:
  bool begin();
  void theme(codexdeck::DeckTheme value);
  void brightness(uint8_t value);
  void dashboard(const codexdeck::TaskStore& tasks, bool bridge, const char* hidStatus, const char* toast);
  void detail(const codexdeck::TaskState& task, size_t scrollLine);
  void keyboard(const char* hidStatus, size_t selected, size_t page, const char* toast);
  void macros(const ControlDeckClient& client, size_t selected);
  void approval(const ApprovalState& approval);
  void textEntry(const char* title, const String& value, bool masked, const char* hint);
  void confirm(const char* title, const char* body);
  void usage(const UsageState& usage);
  void settings(size_t selected, codexdeck::DeckTheme current, uint8_t brightness, const char* hidStatus, bool touchReady);
  void hidSettings(codexdeck::HidTransport transport, const char* status, bool bondKnown, bool bonded,
                   const char* toast);
  void wifiList(const DeckNetwork& network, size_t selected);
  void offline(const DeckNetwork& network, const ControlDeckClient& client);
  void calibration(const TouchSample& sample, const TouchOrientation& orient, bool touchReady);
  void diagnostics(const DiagInfo& info);
  void toastMessage(const char* title, const char* body);
  void present();

  uint16_t width() const { return 368; }
  uint16_t height() const { return 448; }
  bool canvasActive() const { return usingCanvas_; }

  struct Palette {
    uint16_t bg;
    uint16_t panel;
    uint16_t panelAlt;
    uint16_t text;
    uint16_t dim;
    uint16_t accent;
    uint16_t violet;
    uint16_t good;
    uint16_t warn;
    uint16_t bad;
  };

 private:

  Arduino_GFX* display_ = nullptr;
  Palette palette_ = {0x0009, 0x0842, 0x1084, 0xFFFF, 0x8410, 0x07FF, 0xA19F, 0x07E0, 0xFFE0, 0xF800};
  codexdeck::DeckTheme theme_ = codexdeck::DeckTheme::NeonGrid;
  uint8_t brightness_ = 255;
  bool usingCanvas_ = false;
  bool dirty_ = false;

  void background();
  void header(const char* title, const char* status = "");
  void footer(const char* text);
  void panel(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border, uint16_t fill, uint8_t radius = 10);
  void label(const char* value, int16_t x, int16_t y, uint16_t color, uint8_t size = 2);
  void label(const String& value, int16_t x, int16_t y, uint16_t color, uint8_t size = 2) {
    label(value.c_str(), x, y, color, size);
  }
  void wrapped(const char* value, int16_t x, int16_t y, uint8_t chars, uint8_t lines, uint16_t color, uint8_t size = 2);
  void button(int16_t x, int16_t y, int16_t w, int16_t h, const char* value, uint16_t color, bool selected = false);
};
