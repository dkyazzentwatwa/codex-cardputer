#include "hid_keyboard.h"

#if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE == 0
#include <USB.h>
#include <USBHIDKeyboard.h>
#define CODEXDECK_USB_HID_AVAILABLE 1
namespace {
USBHIDKeyboard usbKeyboard;
constexpr uint8_t kUsbEscape = KEY_ESC;
constexpr uint8_t kUsbTab = KEY_TAB;
constexpr uint8_t kUsbLeftControl = KEY_LEFT_CTRL;
constexpr uint8_t kUsbLeftShift = KEY_LEFT_SHIFT;
constexpr uint8_t kUsbLeftAlt = KEY_LEFT_ALT;
constexpr uint8_t kUsbLeftGui = KEY_LEFT_GUI;
}  // namespace
#undef KEY_TAB
#else
#define CODEXDECK_USB_HID_AVAILABLE 0
#endif

#if __has_include(<HijelHID_BLEKeyboard.h>)
#include <HijelHID_BLEKeyboard.h>
#define CODEXDECK_BLE_HID_AVAILABLE 1
#ifndef CODEXDECK_BLE_DEVICE_NAME
#define CODEXDECK_BLE_DEVICE_NAME "CodexDeck Cardputer"
#endif
namespace {
HijelHID_BLEKeyboard bluetoothKeyboard(CODEXDECK_BLE_DEVICE_NAME, "CodexDeck", 100);
}
#else
#define CODEXDECK_BLE_HID_AVAILABLE 0
#endif

namespace {

#if CODEXDECK_USB_HID_AVAILABLE
uint8_t usbHidKey(codexdeck::KeyboardShortcutKey key) {
  using codexdeck::KeyboardShortcutKey;
  switch (key) {
    case KeyboardShortcutKey::Escape: return kUsbEscape;
    case KeyboardShortcutKey::Tab: return kUsbTab;
    case KeyboardShortcutKey::LeftBracket: return '[';
    case KeyboardShortcutKey::RightBracket: return ']';
    case KeyboardShortcutKey::Comma: return ',';
    case KeyboardShortcutKey::D: return 'd';
    case KeyboardShortcutKey::Slash: return '/';
    case KeyboardShortcutKey::Backtick: return '`';
    case KeyboardShortcutKey::A: return 'a';
    case KeyboardShortcutKey::B: return 'b';
    case KeyboardShortcutKey::F: return 'f';
    case KeyboardShortcutKey::G: return 'g';
    case KeyboardShortcutKey::J: return 'j';
    case KeyboardShortcutKey::L: return 'l';
    case KeyboardShortcutKey::N: return 'n';
    case KeyboardShortcutKey::O: return 'o';
    case KeyboardShortcutKey::P: return 'p';
    case KeyboardShortcutKey::S: return 's';
    case KeyboardShortcutKey::T: return 't';
  }
  return 0;
}

void pressUsbModifiers(uint8_t modifiers) {
  if (modifiers & codexdeck::KeyboardModifierControl) usbKeyboard.press(kUsbLeftControl);
  if (modifiers & codexdeck::KeyboardModifierShift) usbKeyboard.press(kUsbLeftShift);
  if (modifiers & codexdeck::KeyboardModifierAlt) usbKeyboard.press(kUsbLeftAlt);
  if (modifiers & codexdeck::KeyboardModifierGui) usbKeyboard.press(kUsbLeftGui);
}
#endif

#if CODEXDECK_BLE_HID_AVAILABLE
uint8_t bluetoothHidKey(codexdeck::KeyboardShortcutKey key) {
  using codexdeck::KeyboardShortcutKey;
  switch (key) {
    case KeyboardShortcutKey::Escape: return KEY_ESCAPE;
    case KeyboardShortcutKey::Tab: return KEY_TAB;
    case KeyboardShortcutKey::LeftBracket: return KEY_LEFTBRACE;
    case KeyboardShortcutKey::RightBracket: return KEY_RIGHTBRACE;
    case KeyboardShortcutKey::Comma: return KEY_COMMA;
    case KeyboardShortcutKey::D: return KEY_D;
    case KeyboardShortcutKey::Slash: return KEY_SLASH;
    case KeyboardShortcutKey::Backtick: return KEY_GRAVE;
    case KeyboardShortcutKey::A: return KEY_A;
    case KeyboardShortcutKey::B: return KEY_B;
    case KeyboardShortcutKey::F: return KEY_F;
    case KeyboardShortcutKey::G: return KEY_G;
    case KeyboardShortcutKey::J: return KEY_J;
    case KeyboardShortcutKey::L: return KEY_L;
    case KeyboardShortcutKey::N: return KEY_N;
    case KeyboardShortcutKey::O: return KEY_O;
    case KeyboardShortcutKey::P: return KEY_P;
    case KeyboardShortcutKey::S: return KEY_S;
    case KeyboardShortcutKey::T: return KEY_T;
  }
  return KEY_NONE;
}

uint8_t bluetoothModifiers(uint8_t modifiers) {
  uint8_t result = 0;
  if (modifiers & codexdeck::KeyboardModifierControl) result |= KEY_MOD_LCTRL;
  if (modifiers & codexdeck::KeyboardModifierShift) result |= KEY_MOD_LSHIFT;
  if (modifiers & codexdeck::KeyboardModifierAlt) result |= KEY_MOD_LALT;
  if (modifiers & codexdeck::KeyboardModifierGui) result |= KEY_MOD_LGUI;
  return result;
}
#endif

}  // namespace

void HidKeyboardController::begin(codexdeck::HidTransport initialTransport) {
  transport_ = codexdeck::hidTransportValid(initialTransport) ? initialTransport : codexdeck::HidTransport::Usb;
#if CODEXDECK_USB_HID_AVAILABLE
  usbKeyboard.begin();
  USB.begin();
  usbAvailable_ = true;
#endif
#if CODEXDECK_BLE_HID_AVAILABLE
  bluetoothAvailable_ = true;
  bluetoothKeyboard.setSecurityMode(HIDSecurity::JustWorks);
  bluetoothKeyboard.setLogLevel(HIDLogLevel::Normal);
#endif
  if (transport_ == codexdeck::HidTransport::Bluetooth) startBluetooth();
  refreshState();
  Serial.printf("[diag] action=hid_begin status=ok transport=%s ready=%d usb=%d bluetooth=%d heap=%u\n",
                codexdeck::hidTransportLabel(transport_), ready_ ? 1 : 0, usbAvailable_ ? 1 : 0,
                bluetoothAvailable_ ? 1 : 0, ESP.getFreeHeap());
}

void HidKeyboardController::startBluetooth() {
#if CODEXDECK_BLE_HID_AVAILABLE
  if (bluetoothRunning_) return;
  bluetoothKeyboard.begin();
  bluetoothInitialized_ = true;
  bluetoothRunning_ = true;
  bluetoothBonded_ = bluetoothKeyboard.isBonded();
  Serial.printf("[diag] action=bluetooth_hid status=advertising name=%s heap=%u\n",
                CODEXDECK_BLE_DEVICE_NAME, ESP.getFreeHeap());
#endif
}

void HidKeyboardController::stopBluetooth() {
#if CODEXDECK_BLE_HID_AVAILABLE
  if (!bluetoothRunning_) return;
  bluetoothKeyboard.releaseAll();
  bluetoothKeyboard.end();
  bluetoothRunning_ = false;
  Serial.printf("[diag] action=bluetooth_hid status=stopped heap=%u\n", ESP.getFreeHeap());
#endif
}

void HidKeyboardController::refreshState() {
#if CODEXDECK_USB_HID_AVAILABLE
  usbReady_ = static_cast<bool>(USB);
#else
  usbReady_ = false;
#endif
  if (transport_ == codexdeck::HidTransport::Usb) {
    ready_ = usbReady_;
    status_ = usbAvailable_ ? (ready_ ? "USB READY" : "PLUG USB") : "USB UNAVAILABLE";
    return;
  }
#if CODEXDECK_BLE_HID_AVAILABLE
  ready_ = bluetoothRunning_ && bluetoothKeyboard.isPaired();
  if (ready_) bluetoothBonded_ = true;
  status_ = ready_ ? "BT READY" : "BT PAIRING";
#else
  ready_ = false;
  status_ = "BT UNAVAILABLE";
#endif
}

bool HidKeyboardController::update() {
  const bool previousReady = ready_;
  const char* previousStatus = status_;
  refreshState();
  if (previousReady == ready_ && previousStatus == status_) return false;
  Serial.printf("[diag] action=hid_state status=changed transport=%s ready=%d bonded=%d heap=%u\n",
                codexdeck::hidTransportLabel(transport_), ready_ ? 1 : 0, bluetoothBonded() ? 1 : 0,
                ESP.getFreeHeap());
  return true;
}

bool HidKeyboardController::available() const {
  return transport_ == codexdeck::HidTransport::Bluetooth ? bluetoothAvailable_ : usbAvailable_;
}

bool HidKeyboardController::ready() const { return ready_; }
codexdeck::HidTransport HidKeyboardController::transport() const { return transport_; }
const char* HidKeyboardController::status() const { return status_; }

bool HidKeyboardController::setTransport(codexdeck::HidTransport transport) {
  if (!codexdeck::hidTransportValid(transport) || transport == transport_) return false;
#if CODEXDECK_USB_HID_AVAILABLE
  if (transport_ == codexdeck::HidTransport::Usb) usbKeyboard.releaseAll();
#endif
  if (transport_ == codexdeck::HidTransport::Bluetooth) stopBluetooth();
  transport_ = transport;
  if (transport_ == codexdeck::HidTransport::Bluetooth) startBluetooth();
  refreshState();
  Serial.printf("[diag] action=hid_transport status=ok transport=%s ready=%d heap=%u\n",
                codexdeck::hidTransportLabel(transport_), ready_ ? 1 : 0, ESP.getFreeHeap());
  return true;
}

bool HidKeyboardController::bluetoothBonded() const {
  return bluetoothBonded_;
}

bool HidKeyboardController::bluetoothStateKnown() const { return bluetoothInitialized_; }

bool HidKeyboardController::clearBluetoothBonds() {
#if CODEXDECK_BLE_HID_AVAILABLE
  if (!bluetoothInitialized_) {
    Serial.println("[diag] action=bluetooth_bonds status=blocked reason=bluetooth_not_initialized");
    return false;
  }
  const bool shouldRestart = transport_ == codexdeck::HidTransport::Bluetooth;
  stopBluetooth();
  const bool hadBond = bluetoothKeyboard.isBonded();
  bluetoothKeyboard.clearBonds();
  bluetoothBonded_ = false;
  if (shouldRestart) startBluetooth();
  refreshState();
  Serial.printf("[diag] action=bluetooth_bonds status=cleared previous=%d advertising=%d heap=%u\n",
                hadBond ? 1 : 0, bluetoothRunning_ ? 1 : 0, ESP.getFreeHeap());
  return hadBond;
#else
  status_ = "BT UNAVAILABLE";
  return false;
#endif
}

bool HidKeyboardController::send(const codexdeck::KeyboardShortcut& shortcut) {
  if (!codexdeck::keyboardShortcutValid(shortcut)) {
#if CODEXDECK_USB_HID_AVAILABLE
    usbKeyboard.releaseAll();
#endif
#if CODEXDECK_BLE_HID_AVAILABLE
    if (bluetoothInitialized_) bluetoothKeyboard.releaseAll();
#endif
    status_ = "INVALID SHORTCUT";
    Serial.printf("[diag] action=hid_send status=blocked id=%s reason=invalid transport=%s\n",
                  shortcut.id ? shortcut.id : "?", codexdeck::hidTransportLabel(transport_));
    return false;
  }
  refreshState();
  if (!ready_) {
    Serial.printf("[diag] action=hid_send status=blocked id=%s reason=not_ready transport=%s\n", shortcut.id,
                  codexdeck::hidTransportLabel(transport_));
    return false;
  }

  if (codexdeck::hidTransportActive(transport_, codexdeck::HidTransport::Usb)) {
#if CODEXDECK_USB_HID_AVAILABLE
    const uint8_t key = usbHidKey(shortcut.key);
    if (key == 0) {
      usbKeyboard.releaseAll();
      status_ = "INVALID SHORTCUT";
      return false;
    }
    pressUsbModifiers(shortcut.modifiers);
    usbKeyboard.press(key);
    delayMicroseconds(900);
    usbKeyboard.releaseAll();
#else
    return false;
#endif
  } else {
#if CODEXDECK_BLE_HID_AVAILABLE
    const uint8_t key = bluetoothHidKey(shortcut.key);
    if (key == KEY_NONE) {
      bluetoothKeyboard.releaseAll();
      status_ = "INVALID SHORTCUT";
      return false;
    }
    bluetoothKeyboard.tap(key, bluetoothModifiers(shortcut.modifiers));
    bluetoothKeyboard.releaseAll();
#else
    return false;
#endif
  }
  Serial.printf("[diag] action=hid_send status=ok id=%s combo=%s transport=%s released=1\n", shortcut.id,
                shortcut.combo, codexdeck::hidTransportLabel(transport_));
  return true;
}
