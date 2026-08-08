#pragma once

#include <Arduino.h>
#include <ControlDeckCore.h>
#include <Preferences.h>

class AmoledSettings {
 public:
  void begin();
  codexdeck::DeckTheme theme() const { return theme_; }
  uint8_t brightness() const { return brightness_; }
  codexdeck::HidTransport hidTransport() const { return hidTransport_; }
  void nextTheme(int direction);
  void nextBrightness(int direction);
  void setHidTransport(codexdeck::HidTransport transport);

  // Touch orientation is stored as the 3-bit TouchOrientation::pack() value so
  // the calibration screen can correct the panel without a firmware reflash.
  uint8_t touchOrientation() const { return touchOrientation_; }
  void setTouchOrientation(uint8_t bits);

  // Until the user explicitly confirms a touch calibration, the firmware boots
  // straight into the calibration screen so a wrong map cannot trap navigation.
  bool touchCalibrated() const { return touchCalibrated_; }
  void markTouchCalibrated();

 private:
  Preferences preferences_;
  codexdeck::DeckTheme theme_ = codexdeck::DeckTheme::NeonGrid;
  uint8_t brightness_ = 255;
  uint8_t touchOrientation_ = 0;
  bool touchCalibrated_ = false;
  codexdeck::HidTransport hidTransport_ = codexdeck::HidTransport::Usb;
  void save();
};
