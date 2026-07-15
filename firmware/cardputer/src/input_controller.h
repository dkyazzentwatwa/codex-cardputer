#pragma once

#include <Arduino.h>

struct InputEvent {
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  bool enterPressed = false;
  bool enterHeld = false;
  bool back = false;
  bool erase = false;
  bool tab = false;
  String text;
};

class InputController {
 public:
  InputEvent read();
};
