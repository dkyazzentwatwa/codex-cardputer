#pragma once

#include <Arduino.h>
#include <ControlDeckCore.h>

class HidKeyboardController {
 public:
  void begin();
  bool update();
  bool available() const;
  bool ready() const;
  bool send(const codexdeck::KeyboardShortcut& shortcut);
  const char* status() const;

 private:
  bool available_ = false;
  bool ready_ = false;
  const char* status_ = "HID unavailable";
};
