#include "hid_keyboard.h"

#if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE == 0
#include <USB.h>
#include <USBHIDKeyboard.h>
#define CODEXDECK_USB_HID_AVAILABLE 1
namespace {
USBHIDKeyboard keyboard;
}
#else
#define CODEXDECK_USB_HID_AVAILABLE 0
#endif

namespace {

uint8_t hidKey(codexdeck::KeyboardShortcutKey key) {
  using codexdeck::KeyboardShortcutKey;
  switch (key) {
    case KeyboardShortcutKey::Escape: return KEY_ESC;
    case KeyboardShortcutKey::Tab: return KEY_TAB;
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

#if CODEXDECK_USB_HID_AVAILABLE
void pressModifiers(uint8_t modifiers) {
  if (modifiers & codexdeck::KeyboardModifierControl) keyboard.press(KEY_LEFT_CTRL);
  if (modifiers & codexdeck::KeyboardModifierShift) keyboard.press(KEY_LEFT_SHIFT);
  if (modifiers & codexdeck::KeyboardModifierAlt) keyboard.press(KEY_LEFT_ALT);
  if (modifiers & codexdeck::KeyboardModifierGui) keyboard.press(KEY_LEFT_GUI);
}
#endif

}  // namespace

void HidKeyboardController::begin() {
#if CODEXDECK_USB_HID_AVAILABLE
  keyboard.begin();
  USB.begin();
  available_ = true;
  ready_ = static_cast<bool>(USB);
  status_ = ready_ ? "HID READY" : "PLUG USB";
  Serial.printf("[diag] action=usb_hid status=ok mode=%d mounted=%d heap=%u\n", ARDUINO_USB_MODE,
                ready_ ? 1 : 0, ESP.getFreeHeap());
#else
  available_ = false;
  ready_ = false;
  status_ = "HID needs USB-OTG";
  Serial.println("[diag] action=usb_hid status=unavailable reason=usb_mode_not_tinyusb");
#endif
}

bool HidKeyboardController::update() {
#if CODEXDECK_USB_HID_AVAILABLE
  const bool nextReady = static_cast<bool>(USB);
  if (nextReady == ready_) return false;
  ready_ = nextReady;
  status_ = ready_ ? "HID READY" : "PLUG USB";
  Serial.printf("[diag] action=usb_hid status=state mounted=%d heap=%u\n", ready_ ? 1 : 0, ESP.getFreeHeap());
  return true;
#else
  return false;
#endif
}

bool HidKeyboardController::available() const { return available_; }
bool HidKeyboardController::ready() const { return ready_; }
const char* HidKeyboardController::status() const { return status_; }

bool HidKeyboardController::send(const codexdeck::KeyboardShortcut& shortcut) {
#if CODEXDECK_USB_HID_AVAILABLE
  if (!ready_) {
    status_ = "PLUG USB";
    Serial.printf("[diag] action=usb_hid_send status=blocked id=%s reason=not_mounted\n", shortcut.id);
    return false;
  }
  const uint8_t key = hidKey(shortcut.key);
  if (!codexdeck::keyboardShortcutValid(shortcut) || key == 0) {
    keyboard.releaseAll();
    status_ = "INVALID SHORTCUT";
    Serial.printf("[diag] action=usb_hid_send status=blocked id=%s reason=invalid\n", shortcut.id ? shortcut.id : "?");
    return false;
  }
  pressModifiers(shortcut.modifiers);
  keyboard.press(key);
  delayMicroseconds(900);
  keyboard.releaseAll();
  Serial.printf("[diag] action=usb_hid_send status=ok id=%s combo=%s released=1\n", shortcut.id, shortcut.combo);
  return true;
#else
  (void)shortcut;
  status_ = "HID needs USB-OTG";
  return false;
#endif
}
