#include "deck_settings.h"

#include <M5Cardputer.h>

namespace {
constexpr const char* kPreferencesNamespace = "codexdeckui";
constexpr const char* kThemeKey = "theme";
constexpr const char* kBrightnessKey = "bright";
constexpr const char* kStatusLightKey = "statusled";
constexpr const char* kPartyLightKey = "partyled";
constexpr const char* kSoundKey = "sound";
constexpr const char* kHidTransportKey = "hidmode";
constexpr uint8_t kBrightnessLevels[] = {96, 160, 255};
constexpr size_t kBrightnessLevelCount = sizeof(kBrightnessLevels) / sizeof(kBrightnessLevels[0]);

size_t nearestBrightnessIndex(uint8_t brightness) {
  size_t best = 0;
  uint16_t bestDistance = 256;
  for (size_t index = 0; index < kBrightnessLevelCount; ++index) {
    const uint16_t distance = brightness > kBrightnessLevels[index] ? brightness - kBrightnessLevels[index]
                                                                     : kBrightnessLevels[index] - brightness;
    if (distance < bestDistance) {
      bestDistance = distance;
      best = index;
    }
  }
  return best;
}
}  // namespace

void DeckSettings::begin() {
  preferences_.begin(kPreferencesNamespace, false);
  const uint8_t savedTheme = preferences_.getUChar(kThemeKey, static_cast<uint8_t>(theme_));
  theme_ = codexdeck::deckThemeValid(static_cast<codexdeck::DeckTheme>(savedTheme))
               ? static_cast<codexdeck::DeckTheme>(savedTheme)
               : codexdeck::DeckTheme::NeonGrid;
  brightness_ = kBrightnessLevels[nearestBrightnessIndex(preferences_.getUChar(kBrightnessKey, 255))];
  statusLightEnabled_ = preferences_.getBool(kStatusLightKey, true);
  partyLightEnabled_ = preferences_.getBool(kPartyLightKey, false);
  soundEnabled_ = preferences_.getBool(kSoundKey, true);
  hidTransport_ = codexdeck::hidTransportFromStored(preferences_.getUChar(kHidTransportKey, 0));
  M5Cardputer.Display.setBrightness(brightness_);
  Serial.printf("[diag] action=settings_load status=ok theme=%s brightness=%u status_led=%s party_led=%s hid=%s\n",
                codexdeck::deckThemeLabel(theme_), brightness_, statusLightEnabled_ ? "on" : "off",
                partyLightEnabled_ ? "on" : "off", codexdeck::hidTransportLabel(hidTransport_));
}

void DeckSettings::cycleTheme(int direction) {
  const size_t count = codexdeck::deckThemeCount();
  size_t index = static_cast<size_t>(theme_);
  if (direction < 0) index = (index + count - 1) % count;
  else index = (index + 1) % count;
  theme_ = codexdeck::deckThemeAt(index);
  saveTheme();
  Serial.printf("[diag] action=settings_theme status=ok theme=%s\n", codexdeck::deckThemeLabel(theme_));
}

void DeckSettings::cycleBrightness(int direction) {
  size_t index = nearestBrightnessIndex(brightness_);
  if (direction < 0) index = (index + kBrightnessLevelCount - 1) % kBrightnessLevelCount;
  else index = (index + 1) % kBrightnessLevelCount;
  brightness_ = kBrightnessLevels[index];
  M5Cardputer.Display.setBrightness(brightness_);
  saveBrightness();
  Serial.printf("[diag] action=settings_brightness status=ok brightness=%u\n", brightness_);
}

void DeckSettings::toggleStatusLight() {
  statusLightEnabled_ = !statusLightEnabled_;
  if (!statusLightEnabled_) {
    partyLightEnabled_ = false;
    savePartyLight();
  }
  saveStatusLight();
  Serial.printf("[diag] action=settings_status_led status=ok enabled=%s\n", statusLightEnabled_ ? "true" : "false");
}

void DeckSettings::togglePartyLight() {
  partyLightEnabled_ = !partyLightEnabled_;
  if (partyLightEnabled_) statusLightEnabled_ = true;
  savePartyLight();
  saveStatusLight();
  Serial.printf("[diag] action=settings_party_led status=ok enabled=%s\n", partyLightEnabled_ ? "true" : "false");
}

void DeckSettings::toggleSound() {
  soundEnabled_ = !soundEnabled_;
  saveSound();
  Serial.printf("[diag] action=settings_sound status=ok enabled=%s\n", soundEnabled_ ? "true" : "false");
}

void DeckSettings::setHidTransport(codexdeck::HidTransport transport) {
  if (!codexdeck::hidTransportValid(transport) || transport == hidTransport_) return;
  hidTransport_ = transport;
  saveHidTransport();
  Serial.printf("[diag] action=settings_hid status=ok transport=%s\n", codexdeck::hidTransportLabel(hidTransport_));
}

void DeckSettings::saveTheme() { preferences_.putUChar(kThemeKey, static_cast<uint8_t>(theme_)); }
void DeckSettings::saveBrightness() { preferences_.putUChar(kBrightnessKey, brightness_); }
void DeckSettings::saveStatusLight() { preferences_.putBool(kStatusLightKey, statusLightEnabled_); }
void DeckSettings::savePartyLight() { preferences_.putBool(kPartyLightKey, partyLightEnabled_); }
void DeckSettings::saveSound() { preferences_.putBool(kSoundKey, soundEnabled_); }
void DeckSettings::saveHidTransport() { preferences_.putUChar(kHidTransportKey, static_cast<uint8_t>(hidTransport_)); }
