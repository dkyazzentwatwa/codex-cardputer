#pragma once

#include <Arduino.h>
#include <ControlDeckCore.h>
#include <Preferences.h>

class DeckSettings {
 public:
  void begin();

  codexdeck::DeckTheme theme() const { return theme_; }
  uint8_t brightness() const { return brightness_; }
  bool statusLightEnabled() const { return statusLightEnabled_; }
  bool partyLightEnabled() const { return partyLightEnabled_; }
  bool soundEnabled() const { return soundEnabled_; }
  void cycleTheme(int direction);
  void cycleBrightness(int direction);
  void toggleStatusLight();
  void togglePartyLight();
  void toggleSound();

 private:
  Preferences preferences_;
  codexdeck::DeckTheme theme_ = codexdeck::DeckTheme::NeonGrid;
  uint8_t brightness_ = 255;
  bool statusLightEnabled_ = true;
  bool partyLightEnabled_ = false;
  bool soundEnabled_ = true;

  void saveTheme();
  void saveBrightness();
  void saveStatusLight();
  void savePartyLight();
  void saveSound();
};
