#include "touch_input.h"

#include <Arduino_DriveBus_Library.h>
#include <Wire.h>
#include <memory>

namespace {
constexpr uint8_t FT3168_ADDRESS = 0x38;
constexpr int TOUCH_SDA = 15;
constexpr int TOUCH_SCL = 14;
constexpr int TOUCH_INT = 21;
constexpr uint16_t WIDTH = 368;
constexpr uint16_t HEIGHT = 448;
constexpr uint16_t SWIPE_THRESHOLD = 42;

// FocalTech FT3x68 touch-data registers (single contiguous block).
constexpr uint8_t FT_REG_STATUS = 0x02;  // [3:0] = active touch points

// DriveBus objects are kept only for the proven power-on/init sequence and the
// device-ID readback used in diagnostics. Coordinate sampling below bypasses
// DriveBus and reads the FT3168 data block in one I2C transaction so X and Y
// always come from the same scan frame (the multi-read path was unreliable).
std::shared_ptr<Arduino_IIC_DriveBus> touchBus;
std::unique_ptr<Arduino_FT3x68> touchDevice;
}  // namespace

bool TouchInput::begin() {
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  touchBus = std::make_shared<Arduino_HWIIC>(TOUCH_SDA, TOUCH_SCL, &Wire);
  touchDevice.reset(new Arduino_FT3x68(touchBus, FT3168_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TOUCH_INT));
  for (uint8_t attempt = 0; attempt < 5; ++attempt) {
    if (touchDevice->begin()) {
      touchDevice->IIC_Write_Device_State(touchDevice->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
                                          touchDevice->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
      deviceId_ = static_cast<uint32_t>(touchDevice->IIC_Read_Device_ID());
      ready_ = true;
      Serial.printf("[diag] action=touch_init status=ok controller=FT3168 addr=0x%02X id=0x%X sda=%d scl=%d int=%d\n",
                    FT3168_ADDRESS, static_cast<unsigned>(deviceId_), TOUCH_SDA, TOUCH_SCL, TOUCH_INT);
      return true;
    }
    delay(400);
  }
  ready_ = false;
  Serial.printf("[diag] action=touch_init status=fail controller=FT3168 addr=0x%02X sda=%d scl=%d int=%d\n",
                FT3168_ADDRESS, TOUCH_SDA, TOUCH_SCL, TOUCH_INT);
  return false;
}

bool TouchInput::readRaw(uint16_t& rawX, uint16_t& rawY) {
  // One transaction: set register pointer to 0x02, then read 5 bytes
  // [status, X_hi, X_lo, Y_hi, Y_lo]. This guarantees a consistent frame.
  Wire.beginTransmission(FT3168_ADDRESS);
  Wire.write(FT_REG_STATUS);
  if (Wire.endTransmission(false) != 0) return false;
  const uint8_t got = Wire.requestFrom(static_cast<int>(FT3168_ADDRESS), 5, static_cast<int>(true));
  if (got < 5) return false;
  const uint8_t status = Wire.read();
  const uint8_t xHi = Wire.read();
  const uint8_t xLo = Wire.read();
  const uint8_t yHi = Wire.read();
  const uint8_t yLo = Wire.read();
  const uint8_t fingers = status & 0x0F;
  if (fingers == 0 || fingers > 2) return false;
  rawX = (static_cast<uint16_t>(xHi & 0x0F) << 8) | xLo;
  rawY = (static_cast<uint16_t>(yHi & 0x0F) << 8) | yLo;
  return true;
}

void TouchInput::mapRaw(uint16_t rawX, uint16_t rawY, uint16_t& x, uint16_t& y) const {
  uint32_t a = rawX;
  uint32_t b = rawY;
  if (orientation_.swapXY) {
    const uint32_t t = a;
    a = b;
    b = t;
  }
  if (a > WIDTH - 1) a = WIDTH - 1;
  if (b > HEIGHT - 1) b = HEIGHT - 1;
  if (orientation_.flipX) a = (WIDTH - 1) - a;
  if (orientation_.flipY) b = (HEIGHT - 1) - b;
  x = static_cast<uint16_t>(a);
  y = static_cast<uint16_t>(b);
}

TouchEvent TouchInput::read() {
  TouchEvent event;
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  const bool pressed = ready_ && readRaw(rawX, rawY);

  uint16_t x = sample_.x;
  uint16_t y = sample_.y;
  if (pressed) mapRaw(rawX, rawY, x, y);

  sample_.pressed = pressed;
  if (pressed) {
    sample_.rawX = rawX;
    sample_.rawY = rawY;
    sample_.x = x;
    sample_.y = y;
  }

  if (pressed && !active_) {
    active_ = true;
    startX_ = lastX_ = x;
    startY_ = lastY_ = y;
    startMs_ = millis();
    return event;
  }
  if (pressed) {
    lastX_ = x;
    lastY_ = y;
    return event;
  }
  if (!active_) return event;
  active_ = false;
  event.x = lastX_;
  event.y = lastY_;
  const int32_t dx = static_cast<int32_t>(lastX_) - startX_;
  const int32_t dy = static_cast<int32_t>(lastY_) - startY_;
  if (abs(dx) < SWIPE_THRESHOLD && abs(dy) < SWIPE_THRESHOLD) {
    event.gesture = TouchGesture::Tap;
    event.x = startX_;
    event.y = startY_;
  } else if (abs(dx) > abs(dy)) {
    event.gesture = dx > 0 ? TouchGesture::SwipeRight : TouchGesture::SwipeLeft;
  } else {
    event.gesture = dy > 0 ? TouchGesture::SwipeDown : TouchGesture::SwipeUp;
  }
  Serial.printf("[diag] action=touch_gesture gesture=%u raw=%u,%u mapped=%u,%u orient=%u dur=%lu\n",
                static_cast<unsigned>(event.gesture), sample_.rawX, sample_.rawY, event.x, event.y,
                orientation_.pack(), millis() - startMs_);
  return event;
}
