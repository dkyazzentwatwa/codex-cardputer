#include "deck_audio.h"

#include <M5Cardputer.h>

void DeckAudio::begin(bool enabled) {
  M5Cardputer.Speaker.begin();
  enabled_ = enabled;
  M5Cardputer.Speaker.setChannelVolume(kChannel, enabled_ ? kVolume : 0);
  Serial.printf("[diag] action=audio_init status=ok enabled=%s channel=%u volume=%u\n", enabled_ ? "true" : "false",
                kChannel, enabled_ ? kVolume : 0);
}

void DeckAudio::setEnabled(bool enabled) {
  enabled_ = enabled;
  M5Cardputer.Speaker.setChannelVolume(kChannel, enabled_ ? kVolume : 0);
}

void DeckAudio::toggle() { setEnabled(!enabled_); }

void DeckAudio::play(DeckTone tone) {
  if (!enabled_) return;
  uint16_t frequency = 880;
  uint16_t durationMs = 22;
  switch (tone) {
    case DeckTone::Move: frequency = 880; durationMs = 22; break;
    case DeckTone::Select: frequency = 1175; durationMs = 45; break;
    case DeckTone::Back: frequency = 523; durationMs = 35; break;
    case DeckTone::Toggle: frequency = 1320; durationMs = 55; break;
    case DeckTone::Error: frequency = 180; durationMs = 90; break;
    case DeckTone::Success: frequency = 1480; durationMs = 65; break;
  }
  M5Cardputer.Speaker.tone(frequency, durationMs, kChannel, true);
}
