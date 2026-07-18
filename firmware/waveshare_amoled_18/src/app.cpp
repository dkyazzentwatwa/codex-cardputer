#include "app.h"

#include <Arduino.h>
#include <cstring>
#include <WiFi.h>

#include "shared_runtime.h"
#include "deck_settings.h"
#include "touch_input.h"
#include "ui_renderer.h"

#if __has_include("secrets.h")
#include "secrets.h"
#define CODEXDECK_LOCAL_SECRETS_PRESENT 1
#pragma message("CodexDeck AMOLED: local Wi-Fi secrets included")
#elif __has_include("../secrets.h")
#include "../secrets.h"
#define CODEXDECK_LOCAL_SECRETS_PRESENT 1
#pragma message("CodexDeck AMOLED: local Wi-Fi secrets included")
#else
#define CODEXDECK_WIFI_SSID ""
#define CODEXDECK_WIFI_PASSWORD ""
#define CODEXDECK_LOCAL_SECRETS_PRESENT 0
#endif

static_assert(!CODEXDECK_LOCAL_SECRETS_PRESENT || sizeof(CODEXDECK_WIFI_SSID) > 1,
              "Local Wi-Fi secrets header is present but has no SSID");

namespace {
enum class Screen : uint8_t {
  Dashboard,
  Offline,
  Detail,
  Macros,
  Keyboard,
  Approval,
  TextEntry,
  ConfirmStop,
  ConfirmClear,
  Usage,
  Settings,
  WifiList,
  Calibration,
  Diagnostics,
};

enum class EntryMode : uint8_t { None, WifiSsid, WifiPassword, Followup };

DeckNetwork network;
ControlDeckClient client;
HidKeyboardController hid;
TouchInput touch;
AmoledRenderer ui;
AmoledSettings settings;
Screen screen = Screen::Offline;
EntryMode entryMode = EntryMode::None;
size_t selected = 0;
size_t macroSelected = 0;
size_t keyboardSelected = 0;
size_t keyboardPage = 0;
size_t settingsSelected = 0;
size_t scrollLine = 0;
size_t wifiSelected = 0;
String entryText;
String selectedSsid;
uint32_t toastUntil = 0;
String localToast;
uint32_t lastRevision = 0;
Screen lastScreen = Screen::Offline;

DiagInfo diag;
uint32_t lastSig = 0;
uint32_t lastHeartbeat = 0;
String wiredBridgeHost;
uint16_t wiredBridgePort = 0;

// mDNS discovery only records the bridge host in DeckNetwork; the websocket
// client stays idle until begin() is called with that host. Hand the freshly
// discovered (or NVS-restored) host to the client so it actually connects.
void syncBridgeClient() {
  const String& host = network.bridgeHost();
  const uint16_t port = network.bridgePort();
  if (host.length() && (host != wiredBridgeHost || port != wiredBridgePort)) {
    wiredBridgeHost = host;
    wiredBridgePort = port;
    client.begin(host, port);
    Serial.printf("[diag] action=bridge_wire status=ok host=%s port=%u\n", host.c_str(), port);
  }
}

const char* decodeWifiStatus(wl_status_t status) {
  switch (status) {
    case WL_CONNECTED: return "connected";
    case WL_NO_SSID_AVAIL: return "ssid_not_found";
    case WL_CONNECT_FAILED: return "auth_failed";
    case WL_CONNECTION_LOST: return "connection_lost";
    case WL_DISCONNECTED: return "disconnected";
    case WL_IDLE_STATUS: return "idle";
#ifdef WL_SCAN_COMPLETED
    case WL_SCAN_COMPLETED: return "scan_completed";
#endif
#ifdef WL_NO_SHIELD
    case WL_NO_SHIELD: return "no_radio";
#endif
    default: return "starting";
  }
}

// Scan before connecting so the panel/serial can prove whether the configured
// SSID is even visible. The SSID text and password are never printed.
void runWifiScanDiag() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(100);
  const int found = WiFi.scanNetworks(false, true);
  diag.ssidSeen = false;
  int bestRssi = -127;
  uint8_t channel = 0;
  bool open = false;
  for (int index = 0; index < found; ++index) {
    if (WiFi.SSID(index) == CODEXDECK_WIFI_SSID && WiFi.RSSI(index) >= bestRssi) {
      bestRssi = WiFi.RSSI(index);
      channel = WiFi.channel(index);
      open = WiFi.encryptionType(index) == WIFI_AUTH_OPEN;
      diag.ssidSeen = true;
    }
  }
  diag.rssi = diag.ssidSeen ? bestRssi : 0;
  diag.channel = diag.ssidSeen ? channel : 0;
  diag.authOpen = open;
  Serial.printf("[diag] action=wifi_scan networks=%d ssid_configured=%d ssid_seen=%d rssi=%d channel=%u auth=%s\n",
                found, strlen(CODEXDECK_WIFI_SSID) > 0, diag.ssidSeen, diag.rssi, diag.channel,
                diag.ssidSeen ? (diag.authOpen ? "open" : "secured") : "-");
  WiFi.scanDelete();
}

void updateDiag() {
  const wl_status_t status = WiFi.status();
  diag.wifiConnected = status == WL_CONNECTED;
  diag.wifiStatus = decodeWifiStatus(status);
  diag.ip = diag.wifiConnected ? WiFi.localIP().toString() : String();
  diag.freeHeap = ESP.getFreeHeap();
  diag.freePsram = ESP.getFreePsram();
  diag.uptimeS = millis() / 1000;
  diag.bridge = client.host();
  diag.bridgeConnected = client.connected();
}

bool isLiveScreen(Screen value) {
  return value == Screen::Calibration || value == Screen::Diagnostics || value == Screen::Offline ||
         value == Screen::WifiList;
}

// Single frame signature: the UI repaints only when this changes, so idle
// screens never repaint and touch samples that change nothing never flicker.
// Live screens fold in a ~12 fps time tick so markers/counters still update.
uint32_t frameSig(uint32_t now) {
  uint32_t hash = 2166136261u;
  auto mix = [&](uint32_t value) { hash ^= value; hash *= 16777619u; };
  mix(static_cast<uint32_t>(screen));
  mix(selected);
  mix(macroSelected);
  mix(keyboardSelected);
  mix(keyboardPage);
  mix(settingsSelected);
  mix(scrollLine);
  mix(wifiSelected);
  mix(static_cast<uint32_t>(entryMode));
  for (size_t i = 0; i < entryText.length(); ++i) mix(static_cast<uint8_t>(entryText[i]));
  mix(settings.touchOrientation());
  mix(client.revision());
  mix(client.connected() ? 1u : 2u);
  mix(hid.ready() ? 1u : 2u);
  mix(static_cast<uint32_t>(WiFi.status()));
  const char* activeToast = localToast.length() && now < toastUntil ? localToast.c_str() : client.toast();
  for (const char* p = activeToast; *p; ++p) mix(static_cast<uint8_t>(*p));
  if (isLiveScreen(screen)) {
    mix(now / 80);
    if (screen == Screen::Calibration) {
      const TouchSample& s = touch.sample();
      mix(s.pressed ? (0x10000u | (static_cast<uint32_t>(s.x) << 8) | s.y) : 0u);
    }
  }
  return hash;
}

void emitHeartbeat(uint32_t now) {
  const TouchSample& s = touch.sample();
  Serial.printf("[hb] up=%lus heap=%u psram=%u wifi=%s ip=%s bridge=%s conn=%d canvas=%s touch=%s raw=%u,%u map=%u,%u screen=%u\n",
                now / 1000, ESP.getFreeHeap(), ESP.getFreePsram(), decodeWifiStatus(WiFi.status()),
                WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-",
                client.host().length() ? client.host().c_str() : "-", client.connected() ? 1 : 0,
                ui.canvasActive() ? "psram" : "direct", touch.available() ? "ok" : "missing",
                s.rawX, s.rawY, s.x, s.y, static_cast<unsigned>(screen));
}

void showToast(const String& message) {
  localToast = message;
  toastUntil = millis() + 3000;
}

void openTextEntry(EntryMode mode, const String& value = String()) {
  entryMode = mode;
  entryText = value;
  screen = Screen::TextEntry;
}

void openWifi() {
  network.scan();
  wifiSelected = 0;
  screen = Screen::WifiList;
}

void sendShortcut(size_t index) {
  const auto* shortcut = codexdeck::keyboardShortcutAt(index);
  if (!shortcut) return;
  if (hid.send(*shortcut)) showToast(String("Sent ") + shortcut->label);
  else showToast(hid.status());
}

void handleTextTap(uint16_t x, uint16_t y) {
  if (y >= 355 && y < 410) {
    if (x < 112) {
      if (entryText.length()) entryText.remove(entryText.length() - 1);
    } else if (x < 224) {
      if (entryText.length() < 240) entryText += ' ';
    } else {
      if (entryMode == EntryMode::WifiSsid && entryText.length()) {
        selectedSsid = entryText;
        openTextEntry(EntryMode::WifiPassword);
      } else if (entryMode == EntryMode::WifiPassword) {
        network.connectTo(selectedSsid, entryText);
        entryText = "";
        screen = Screen::Offline;
      } else if (entryMode == EntryMode::Followup && entryText.length()) {
        const auto* task = client.tasks().selected();
        if (task) client.sendFollowup(task->id, entryText);
        entryText = "";
        screen = Screen::Detail;
      }
    }
    return;
  }
  if (y < 155 || y >= 347) return;
  const char* rows[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  const uint8_t row = static_cast<uint8_t>((y - 155) / 48);
  const size_t count = strlen(rows[row]);
  const int startX = (368 - static_cast<int>(count) * 32) / 2;
  const int column = (static_cast<int>(x) - startX) / 32;
  if (column >= 0 && static_cast<size_t>(column) < count && entryText.length() < 240) entryText += rows[row][column];
}

void handleTouch(const TouchEvent& event) {
  if (event.gesture == TouchGesture::None) return;
  if (screen == Screen::TextEntry) {
    if (event.gesture == TouchGesture::Tap) handleTextTap(event.x, event.y);
    else if (event.gesture == TouchGesture::SwipeRight) screen = Screen::Dashboard;
    return;
  }
  if (event.gesture == TouchGesture::SwipeUp) {
    if (screen == Screen::Keyboard && keyboardPage + 1 < codexdeck::keyboardShortcutPageCount()) {
      ++keyboardPage; keyboardSelected = codexdeck::keyboardShortcutPageStart(keyboardPage);
    } else if (screen == Screen::Dashboard && client.tasks().count()) client.tasks().moveSelection(1);
    else if (screen == Screen::Detail) scrollLine += 5;
    else if (screen == Screen::Settings) settingsSelected = (settingsSelected + 3) % 4;
    return;
  }
  if (event.gesture == TouchGesture::SwipeDown) {
    if (screen == Screen::Keyboard && keyboardPage > 0) {
      --keyboardPage; keyboardSelected = codexdeck::keyboardShortcutPageStart(keyboardPage);
    } else if (screen == Screen::Dashboard && client.tasks().count()) client.tasks().moveSelection(-1);
    else if (screen == Screen::Detail && scrollLine >= 5) scrollLine -= 5;
    else if (screen == Screen::Settings) settingsSelected = (settingsSelected + 1) % 4;
    return;
  }
  if (event.gesture == TouchGesture::SwipeLeft && screen == Screen::Keyboard && keyboardPage + 1 < codexdeck::keyboardShortcutPageCount()) {
    ++keyboardPage; keyboardSelected = codexdeck::keyboardShortcutPageStart(keyboardPage); return;
  }
  if (event.gesture == TouchGesture::SwipeRight && screen == Screen::Keyboard && keyboardPage > 0) {
    --keyboardPage; keyboardSelected = codexdeck::keyboardShortcutPageStart(keyboardPage); return;
  }
  if (event.gesture != TouchGesture::Tap) return;

  const uint16_t x = event.x;
  const uint16_t y = event.y;
  if (screen == Screen::Dashboard) {
    if (y >= 370 && y < 410) {
      if (x < 116) { screen = Screen::Macros; macroSelected = 0; }
      else if (x < 232) { screen = Screen::Keyboard; keyboardPage = 0; keyboardSelected = 0; }
      else { screen = Screen::Settings; settingsSelected = 0; }
    } else if (y >= 140 && y < 386 && client.tasks().count()) {
      const size_t index = min<size_t>(client.tasks().count() - 1, (y - 140) / 82);
      client.tasks().select(index); scrollLine = 0; screen = Screen::Detail;
    }
  } else if (screen == Screen::Offline) {
    if (y >= 340) {
      if (x < 124) {
        screen = Screen::Keyboard;
        keyboardPage = 0;
        keyboardSelected = 0;
      } else if (x < 242) {
        openWifi();
      } else {
        screen = Screen::Settings;
        settingsSelected = 0;
      }
    } else {
      // Tapping the offline status area jumps straight to on-screen diagnostics
      // (Wi-Fi scan result and decoded failure reason) — the reason you're here.
      screen = Screen::Diagnostics;
    }
  } else if (screen == Screen::Detail) {
    if (y > 360 && x < 120) screen = Screen::Dashboard;
    else if (y > 360 && x < 235) openTextEntry(EntryMode::Followup);
    else if (y > 360) screen = Screen::ConfirmStop;
  } else if (screen == Screen::Keyboard) {
    const size_t start = codexdeck::keyboardShortcutPageStart(keyboardPage);
    const size_t count = codexdeck::keyboardShortcutPageItemCount(keyboardPage);
    if (y >= 112 && y < 344) {
      const size_t item = (y - 112) / 58 * 2 + (x >= 184 ? 1 : 0);
      if (item < count) { keyboardSelected = start + item; sendShortcut(keyboardSelected); }
    } else if (y > 360 && x < 120) screen = Screen::Dashboard;
  } else if (screen == Screen::Macros) {
    if (y > 360) screen = Screen::Dashboard;
    else if (y >= 82 && y < 370) {
      const size_t index = (y - 82) / 48;
      const auto* macro = client.globalMacro(index);
      if (macro && macro->enabled) {
        if (strcmp(macro->action, "launch_skill") == 0) client.sendSkill(macro->id);
        else client.sendWorkflow(macro->id);
        screen = Screen::Dashboard;
      }
    }
  } else if (screen == Screen::Approval) {
    if (y > 320 && x < 120) client.sendApproval(client.approval().id, "accept");
    else if (y > 320 && x < 240) client.sendApproval(client.approval().id, "decline");
    else if (y > 320) client.sendApproval(client.approval().id, "cancel");
    screen = Screen::Detail;
  } else if (screen == Screen::ConfirmClear || screen == Screen::ConfirmStop) {
    if (y > 275 && x > 180) {
      if (screen == Screen::ConfirmClear) client.sendClearFinished();
      else if (client.tasks().selected()) client.sendStop(client.tasks().selected()->id);
    }
    screen = Screen::Dashboard;
  } else if (screen == Screen::Usage) {
    if (y > 350) screen = Screen::Dashboard;
  } else if (screen == Screen::Settings) {
    if (y >= 84 && y < 340) {
      settingsSelected = min<size_t>(3, (y - 84) / 62);
      if (settingsSelected == 0) { settings.nextTheme(1); ui.theme(settings.theme()); }
      else if (settingsSelected == 1) { settings.nextBrightness(1); ui.brightness(settings.brightness()); }
      else if (settingsSelected == 3) screen = Screen::Calibration;
    } else if (y >= 372) {
      if (x < 94) openWifi();
      else if (x < 184) screen = Screen::Diagnostics;
      else if (x < 268) screen = Screen::Calibration;
      else screen = client.connected() ? Screen::Dashboard : Screen::Offline;
    }
  } else if (screen == Screen::Calibration) {
    TouchOrientation orientation = touch.orientation();
    if (y >= 332 && y < 366) {
      if (x < 92) orientation.swapXY = !orientation.swapXY;
      else if (x < 178) orientation.flipX = !orientation.flipX;
      else if (x < 264) orientation.flipY = !orientation.flipY;
      if (x < 264) {
        touch.setOrientation(orientation);
        settings.setTouchOrientation(orientation.pack());
      } else {
        settings.markTouchCalibrated();
        showToast("Calibration saved");
      }
    } else if (y >= 374) {
      if (x < 132) screen = Screen::Settings;
      else if (x < 260) {
        orientation = TouchOrientation();
        touch.setOrientation(orientation);
        settings.setTouchOrientation(orientation.pack());
        showToast("Orientation reset");
      }
    }
  } else if (screen == Screen::Diagnostics) {
    if (y >= 372) {
      if (x < 142) screen = client.connected() ? Screen::Dashboard : Screen::Offline;
      else screen = Screen::Calibration;
    }
  } else if (screen == Screen::WifiList) {
    if (y > 365 && x > 180) screen = Screen::Dashboard;
    else if (y > 365) openWifi();
    else if (y >= 82 && y < 360 && network.networkCount()) {
      wifiSelected = min<size_t>(network.networkCount() - 1, (y - 82) / 45);
      const auto* entry = network.networkAt(wifiSelected);
      selectedSsid = entry->ssid;
      if (entry->secured) openTextEntry(EntryMode::WifiPassword);
      else { network.connectTo(selectedSsid, ""); screen = Screen::Offline; }
    }
  }
}

void render() {
  const char* toast = localToast.length() && millis() < toastUntil ? localToast.c_str() : client.toast();
  switch (screen) {
    case Screen::Dashboard: ui.dashboard(client.tasks(), client.connected(), hid.ready(), toast); break;
    case Screen::Offline: ui.offline(network, client); break;
    case Screen::Detail:
      if (client.tasks().selected()) ui.detail(*client.tasks().selected(), scrollLine); else screen = Screen::Dashboard;
      break;
    case Screen::Macros: ui.macros(client, macroSelected); break;
    case Screen::Keyboard: ui.keyboard(hid.status(), keyboardSelected, keyboardPage, toast); break;
    case Screen::Approval: ui.approval(client.approval()); break;
    case Screen::TextEntry:
      ui.textEntry(entryMode == EntryMode::WifiSsid ? "WI-FI SSID" : entryMode == EntryMode::WifiPassword ? "WI-FI PASSWORD" : "FOLLOW-UP",
                   entryText, entryMode == EntryMode::WifiPassword, entryMode == EntryMode::Followup ? "Type follow-up" : "Type network name");
      break;
    case Screen::ConfirmClear: ui.confirm("CLEAR FINISHED?", "Remove terminal task history? Active and stale tasks remain."); break;
    case Screen::ConfirmStop: ui.confirm("STOP TASK?", "Interrupt the selected Codex turn?"); break;
    case Screen::Usage: ui.usage(client.usage()); break;
    case Screen::Settings: ui.settings(settingsSelected, settings.theme(), settings.brightness(), hid.ready(), touch.available()); break;
    case Screen::WifiList: ui.wifiList(network, wifiSelected); break;
    case Screen::Calibration: ui.calibration(touch.sample(), touch.orientation(), touch.available()); break;
    case Screen::Diagnostics: updateDiag(); ui.diagnostics(diag); break;
  }
  ui.present();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialWait = millis();
  while (!Serial && millis() - serialWait < 1500) delay(10);
  delay(200);
  Serial.println();
  Serial.printf("[diag] action=boot fw=waveshare_amoled_18 secrets=%d ssid_len=%u\n",
                CODEXDECK_LOCAL_SECRETS_PRESENT, static_cast<unsigned>(strlen(CODEXDECK_WIFI_SSID)));

  settings.begin();
  ui.theme(settings.theme());
  ui.brightness(settings.brightness());
  ui.begin();
  hid.begin();
  touch.begin();
  touch.setOrientation(TouchOrientation::unpack(settings.touchOrientation()));

  diag.secretsPresent = CODEXDECK_LOCAL_SECRETS_PRESENT;
  diag.ssidLen = strlen(CODEXDECK_WIFI_SSID);
  diag.touchReady = touch.available();
  diag.touchId = touch.deviceId();
  diag.canvasPsram = ui.canvasActive();

  // Prove SSID visibility before attempting the association.
  runWifiScanDiag();

  network.begin();
  if (strlen(CODEXDECK_WIFI_SSID) > 0) {
    WiFi.setSleep(false);
    // The board-local secrets file is the provisioning source of truth: apply
    // it on every boot so a stale NVS value cannot silently block the network.
    network.connectTo(CODEXDECK_WIFI_SSID, CODEXDECK_WIFI_PASSWORD);
    Serial.println("[diag] action=wifi_credentials status=loaded source=local_secrets");
  } else {
    Serial.println("[diag] action=wifi_credentials status=absent");
  }

  // Show a live "connecting" frame instead of a frozen splash during the wait.
  screen = Screen::Offline;
  render();

  // Non-blocking connect window: keep touch + HID responsive while the radio
  // associates, and log each status transition with a decoded reason.
  const uint32_t started = millis();
  wl_status_t last = static_cast<wl_status_t>(0xFF);
  uint32_t lastBootPaint = 0;
  while (millis() - started < 8000 && WiFi.status() != WL_CONNECTED) {
    network.update();
    syncBridgeClient();
    client.update(network.connected());
    hid.update();
    const wl_status_t status = WiFi.status();
    if (status != last) {
      Serial.printf("[diag] action=wifi_state status=%s code=%d t=%lums\n", decodeWifiStatus(status), status,
                    millis() - started);
      last = status;
    }
    if (millis() - lastBootPaint >= 500) {
      render();
      lastBootPaint = millis();
    }
    delay(50);
  }
  Serial.printf("[diag] action=wifi_result connected=%d code=%d status=%s ip=%s ms=%lu\n",
                WiFi.status() == WL_CONNECTED, WiFi.status(), decodeWifiStatus(WiFi.status()),
                WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-", millis() - started);

  screen = client.connected() ? Screen::Dashboard : Screen::Offline;
  // First boot (no confirmed calibration): land on the calibration screen so a
  // wrong touch map cannot trap navigation. Cleared by the SAVE button.
  if (!settings.touchCalibrated()) screen = Screen::Calibration;
  updateDiag();
  render();
  lastSig = frameSig(millis());
  lastScreen = screen;
  lastRevision = client.revision();
}

void loop() {
  const uint32_t now = millis();
  network.update();
  syncBridgeClient();
  client.update(network.connected());
  hid.update();

  // Bridge presence drives the Dashboard/Offline split; it never gates HID.
  if (client.connected() && screen == Screen::Offline) screen = Screen::Dashboard;
  if (!client.connected() && screen == Screen::Dashboard) screen = Screen::Offline;

  const TouchEvent event = touch.read();
  handleTouch(event);
  if (localToast.length() && now >= toastUntil) localToast = "";

  // Repaint only when the composed frame actually changes: idle screens and
  // touch samples that change nothing produce no repaint (the flicker fix);
  // live screens fold a time tick into the signature so markers/counters move.
  const uint32_t sig = frameSig(now);
  if (sig != lastSig) {
    render();
    lastSig = sig;
    lastScreen = screen;
    lastRevision = client.revision();
  }

  if (now - lastHeartbeat >= 1000) {
    emitHeartbeat(now);
    lastHeartbeat = now;
  }
  delay(15);
}
