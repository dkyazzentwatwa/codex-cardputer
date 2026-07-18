#pragma once

#include <Arduino.h>

enum class TouchGesture : uint8_t {
  None,
  Tap,
  SwipeUp,
  SwipeDown,
  SwipeLeft,
  SwipeRight,
};

struct TouchEvent {
  TouchGesture gesture = TouchGesture::None;
  uint16_t x = 0;
  uint16_t y = 0;
};

// Live finger sample, updated every read(). Used by the calibration/debug
// screen to draw a marker under the finger and show raw-vs-mapped drift.
struct TouchSample {
  bool pressed = false;
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  uint16_t x = 0;  // mapped to panel coordinates
  uint16_t y = 0;
};

// Orientation transform applied to raw FT3168 coordinates. Three bits cover all
// eight rotation/mirror combinations; persisted so the calibration screen can
// correct the panel without a reflash. Default is identity (raw == panel).
struct TouchOrientation {
  bool swapXY = false;
  bool flipX = false;
  bool flipY = false;

  uint8_t pack() const {
    return static_cast<uint8_t>((swapXY ? 1 : 0) | (flipX ? 2 : 0) | (flipY ? 4 : 0));
  }
  static TouchOrientation unpack(uint8_t bits) {
    TouchOrientation o;
    o.swapXY = bits & 1;
    o.flipX = bits & 2;
    o.flipY = bits & 4;
    return o;
  }
};

class TouchInput {
 public:
  bool begin();
  // Poll the controller and return a gesture on finger release. Call once per
  // loop; sample() is refreshed as a side effect for live-marker rendering.
  TouchEvent read();
  const TouchSample& sample() const { return sample_; }
  bool available() const { return ready_; }
  uint32_t deviceId() const { return deviceId_; }

  void setOrientation(const TouchOrientation& orientation) { orientation_ = orientation; }
  const TouchOrientation& orientation() const { return orientation_; }

 private:
  bool ready_ = false;
  bool active_ = false;
  uint16_t startX_ = 0;
  uint16_t startY_ = 0;
  uint16_t lastX_ = 0;
  uint16_t lastY_ = 0;
  uint32_t startMs_ = 0;
  uint32_t deviceId_ = 0;
  TouchSample sample_;
  TouchOrientation orientation_;

  // Direct single-transaction FT3168 read (proven FocalTech register burst).
  bool readRaw(uint16_t& rawX, uint16_t& rawY);
  void mapRaw(uint16_t rawX, uint16_t rawY, uint16_t& x, uint16_t& y) const;
};
