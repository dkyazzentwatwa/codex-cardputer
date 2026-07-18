#pragma once

#include <Arduino.h>

enum class DeckTone : uint8_t {
  Move,
  Select,
  Back,
  Toggle,
  Error,
  Success,
};

class DeckAudio {
 public:
  void begin(bool enabled);
  void setEnabled(bool enabled);
  void toggle();
  bool enabled() const { return enabled_; }
  void play(DeckTone tone);

 private:
  static constexpr uint8_t kChannel = 1;
  static constexpr uint8_t kVolume = 88;
  bool enabled_ = true;
};
