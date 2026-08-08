#include "deck_settings.h"

namespace {
constexpr uint8_t kBrightness[] = {96, 160, 255};
constexpr size_t kBrightnessCount = sizeof(kBrightness) / sizeof(kBrightness[0]);
}

void AmoledSettings::begin() {
  preferences_.begin("codexamoled", false);
  const uint8_t savedTheme = preferences_.getUChar("theme", 0);
  theme_ = codexdeck::deckThemeValid(static_cast<codexdeck::DeckTheme>(savedTheme))
               ? static_cast<codexdeck::DeckTheme>(savedTheme)
               : codexdeck::DeckTheme::NeonGrid;
  brightness_ = preferences_.getUChar("bright", 255);
  touchOrientation_ = preferences_.getUChar("touchori", 0) & 0x07;
  touchCalibrated_ = preferences_.getBool("touchcal", false);
  hidTransport_ = codexdeck::hidTransportFromStored(preferences_.getUChar("hidmode", 0));
  Serial.printf("[diag] action=amoled_settings status=ok theme=%s brightness=%u touch_orient=%u touch_cal=%d hid=%s\n",
                codexdeck::deckThemeLabel(theme_), brightness_, touchOrientation_, touchCalibrated_,
                codexdeck::hidTransportLabel(hidTransport_));
}

void AmoledSettings::setTouchOrientation(uint8_t bits) {
  touchOrientation_ = bits & 0x07;
  preferences_.putUChar("touchori", touchOrientation_);
}

void AmoledSettings::markTouchCalibrated() {
  touchCalibrated_ = true;
  preferences_.putBool("touchcal", true);
}

void AmoledSettings::nextTheme(int direction) {
  const size_t count = codexdeck::deckThemeCount();
  size_t index = static_cast<size_t>(theme_);
  index = direction < 0 ? (index + count - 1) % count : (index + 1) % count;
  theme_ = codexdeck::deckThemeAt(index);
  save();
}

void AmoledSettings::nextBrightness(int direction) {
  size_t index = 0;
  for (size_t i = 0; i < kBrightnessCount; ++i) if (brightness_ == kBrightness[i]) index = i;
  index = direction < 0 ? (index + kBrightnessCount - 1) % kBrightnessCount : (index + 1) % kBrightnessCount;
  brightness_ = kBrightness[index];
  save();
}

void AmoledSettings::setHidTransport(codexdeck::HidTransport transport) {
  if (!codexdeck::hidTransportValid(transport) || transport == hidTransport_) return;
  hidTransport_ = transport;
  preferences_.putUChar("hidmode", static_cast<uint8_t>(hidTransport_));
  Serial.printf("[diag] action=amoled_settings_hid status=ok transport=%s\n",
                codexdeck::hidTransportLabel(hidTransport_));
}

void AmoledSettings::save() {
  preferences_.putUChar("theme", static_cast<uint8_t>(theme_));
  preferences_.putUChar("bright", brightness_);
}
