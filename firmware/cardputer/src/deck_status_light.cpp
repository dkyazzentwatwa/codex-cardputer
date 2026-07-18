#include "deck_status_light.h"

#include <Adafruit_NeoPixel.h>

namespace {
constexpr uint8_t kStatusLedPowerPin = 38;
constexpr uint8_t kStatusLedDataPin = 21;
constexpr uint8_t kStatusLedBrightness = 50;
Adafruit_NeoPixel statusLed(1, kStatusLedDataPin, NEO_GRB + NEO_KHZ800);

bool isCompleted(codexdeck::TaskStatus status) { return status == codexdeck::TaskStatus::Completed; }

bool needsAttention(const codexdeck::TaskStore& tasks) {
  for (size_t index = 0; index < tasks.count(); ++index) {
    const codexdeck::TaskState* task = tasks.at(index);
    if (task && task->requiresAttention) return true;
  }
  return false;
}

bool hasActiveTask(const codexdeck::TaskStore& tasks) {
  for (size_t index = 0; index < tasks.count(); ++index) {
    const codexdeck::TaskState* task = tasks.at(index);
    if (!task) continue;
    if (task->status == codexdeck::TaskStatus::Starting || task->status == codexdeck::TaskStatus::Running)
      return true;
  }
  return false;
}
}  // namespace

void DeckStatusLight::begin(bool enabled) {
  pinMode(kStatusLedPowerPin, OUTPUT);
  digitalWrite(kStatusLedPowerPin, HIGH);
  statusLed.begin();
  statusLed.setBrightness(kStatusLedBrightness);
  statusLed.clear();
  statusLed.show();
  available_ = true;
  enabled_ = enabled;
  outputRed_ = outputGreen_ = outputBlue_ = 255;
  setColor(0, 0, 0);
  Serial.printf("[diag] action=status_led status=%s enabled=%s\n", available_ ? "ready" : "unavailable",
                enabled_ ? "true" : "false");
}

void DeckStatusLight::setEnabled(bool enabled) {
  enabled_ = enabled;
  pulseUntilMs_ = 0;
  if (!enabled_) setColor(0, 0, 0);
}

void DeckStatusLight::setPartyMode(bool enabled) {
  partyMode_ = enabled;
  pulseUntilMs_ = 0;
  Serial.printf("[diag] action=status_led_party status=ok enabled=%s\n", partyMode_ ? "true" : "false");
}

void DeckStatusLight::pulse(uint8_t red, uint8_t green, uint8_t blue, uint16_t durationMs) {
  if (!enabled_) return;
  pulseUntilMs_ = millis() + durationMs;
  setColor(red, green, blue);
}

void DeckStatusLight::update(const DeckNetwork& network, const ControlDeckClient& client, bool hidReady, bool macroPadOpen) {
  const codexdeck::TaskStore& tasks = client.tasks();
  if (!snapshotsInitialized_) {
    rememberTasks(tasks);
    snapshotsInitialized_ = true;
  } else if (sawNewCompletion(tasks)) {
    pulse(255, 255, 255, 180);
    rememberTasks(tasks);
  } else if (taskSnapshotCount_ != tasks.count()) {
    rememberTasks(tasks);
  }

  if (!enabled_ || millis() < pulseUntilMs_) return;
  if (partyMode_) {
    constexpr uint8_t kPartyColors[][3] = {
        {255, 0, 30}, {255, 85, 0}, {255, 240, 0}, {0, 255, 45}, {0, 150, 255}, {165, 45, 255},
    };
    constexpr size_t kPartyColorCount = sizeof(kPartyColors) / sizeof(kPartyColors[0]);
    const uint8_t* color = kPartyColors[(millis() / 450) % kPartyColorCount];
    setColor(color[0], color[1], color[2]);
    return;
  }
  if (client.approval().open || needsAttention(tasks)) {
    setColor(255, 118, 0);  // amber: approval or attention
  } else if (macroPadOpen) {
    setColor(165, 45, 255);  // violet: macro pad
  } else if (!network.connected()) {
    setColor(255, 0, 0);  // red: Wi-Fi disconnected
  } else if (!client.connected()) {
    setColor(0, 190, 255);  // cyan: bridge discovery
  } else if (hasActiveTask(tasks)) {
    setColor(0, 190, 255);  // cyan: Codex agent active
  } else if (hidReady) {
    setColor(0, 255, 40);  // green: bridge and HID ready
  } else {
    setColor(0, 190, 255);  // cyan: bridge connected, waiting on USB host
  }
}

void DeckStatusLight::setColor(uint8_t red, uint8_t green, uint8_t blue) {
  if (!enabled_) red = green = blue = 0;
  if (red == outputRed_ && green == outputGreen_ && blue == outputBlue_) return;
  outputRed_ = red;
  outputGreen_ = green;
  outputBlue_ = blue;
  if (!available_) return;
  digitalWrite(kStatusLedPowerPin, HIGH);
  statusLed.setPixelColor(0, statusLed.Color(red, green, blue));
  statusLed.show();
  if (!enabled_) digitalWrite(kStatusLedPowerPin, LOW);
}

void DeckStatusLight::rememberTasks(const codexdeck::TaskStore& tasks) {
  taskSnapshotCount_ = tasks.count();
  for (size_t index = 0; index < taskSnapshotCount_; ++index) {
    const codexdeck::TaskState* task = tasks.at(index);
    if (!task) continue;
    codexdeck::copyText(taskSnapshots_[index].id, sizeof(taskSnapshots_[index].id), task->id);
    taskSnapshots_[index].status = task->status;
  }
}

bool DeckStatusLight::sawNewCompletion(const codexdeck::TaskStore& tasks) const {
  for (size_t index = 0; index < tasks.count(); ++index) {
    const codexdeck::TaskState* task = tasks.at(index);
    if (!task || !isCompleted(task->status)) continue;
    bool found = false;
    for (size_t previous = 0; previous < taskSnapshotCount_; ++previous) {
      if (strcmp(taskSnapshots_[previous].id, task->id) == 0) {
        found = true;
        if (!isCompleted(taskSnapshots_[previous].status)) return true;
        break;
      }
    }
    if (!found) return true;
  }
  return false;
}
