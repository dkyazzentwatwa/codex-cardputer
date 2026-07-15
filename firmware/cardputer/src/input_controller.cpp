#include "input_controller.h"

#include <M5Cardputer.h>

InputEvent InputController::read() {
  InputEvent event;
  const bool changed = M5Cardputer.Keyboard.isChange();
  const bool pressed = M5Cardputer.Keyboard.isPressed();
  Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
  event.enterHeld = pressed && keys.enter;
  if (!changed || !pressed) return event;

  event.enterPressed = keys.enter;
  event.erase = keys.del;
  event.tab = keys.tab;
  for (const auto& key : M5Cardputer.Keyboard.keyList()) {
    if (key.y == 3 && key.x == 10) event.left = true;
    if (key.y == 3 && key.x == 12) event.right = true;
    if (key.y == 2 && key.x == 11) event.up = true;
    if (key.y == 3 && key.x == 11) event.down = true;
  }
  for (char character : keys.word) {
    if (character == '`' || character == 0x1B) event.back = true;
    else if (character == ';' || character == ',') event.up = true;
    else if (character == '.' || character == '/') event.down = true;
    if (character >= 32 && character <= 126) event.text += character;
  }
  return event;
}
