#pragma once

#include <Arduino.h>

#include "control_deck_client.h"
#include "network_manager.h"

class DeckStatusLight {
 public:
  void begin(bool enabled);
  void setEnabled(bool enabled);
  void setPartyMode(bool enabled);
  void pulse(uint8_t red, uint8_t green, uint8_t blue, uint16_t durationMs = 220);
  void update(const DeckNetwork& network, const ControlDeckClient& client, bool hidReady, bool macroPadOpen);

 private:
  struct TaskSnapshot {
    char id[129] = {};
    codexdeck::TaskStatus status = codexdeck::TaskStatus::Starting;
  };

  bool enabled_ = true;
  bool partyMode_ = false;
  bool available_ = false;
  bool snapshotsInitialized_ = false;
  TaskSnapshot taskSnapshots_[codexdeck::MAX_TASKS] = {};
  size_t taskSnapshotCount_ = 0;
  uint8_t outputRed_ = 0;
  uint8_t outputGreen_ = 0;
  uint8_t outputBlue_ = 0;
  uint32_t pulseUntilMs_ = 0;

  void setColor(uint8_t red, uint8_t green, uint8_t blue);
  void rememberTasks(const codexdeck::TaskStore& tasks);
  bool sawNewCompletion(const codexdeck::TaskStore& tasks) const;
};
