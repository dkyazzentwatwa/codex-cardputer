# Waveshare AMOLED 1.8 target

The `firmware/waveshare_amoled_18` target is the portrait 368×448 ESP32-S3
port for the SH8601 AMOLED and FT3168 touch controller. It keeps the
CodexDeck bridge protocol and shortcut catalog shared with Cardputer while
using a touch-first renderer.

Build and upload with Arduino CLI only:

```sh
./tools/build-waveshare-amoled.sh
./tools/flash-waveshare-amoled.sh /dev/cu.usbmodemXXXX
```

The board profile uses 16 MB flash, OPI PSRAM, TinyUSB CDC/HID, Arduino GFX,
Adafruit XCA9554, ArduinoJson, and WebSockets. FT3168 is accessed through the
small raw register adapter in `src/touch_input.cpp`; this keeps the target
offline-buildable while matching the proven DriveBus pin map (SDA 15, SCL 14,
INT 21). The display power sequence is XCA9554 at `0x20`, followed by SH8601
QSPI on GPIO 4–7, 11, and 12.

The ignored `secrets.h` file may provide `CODEXDECK_WIFI_SSID` and
`CODEXDECK_WIFI_PASSWORD`. Saved Preferences credentials take precedence.
Values are never printed to serial.

The profile supports the same exclusive USB/Bluetooth HID selection as the
Cardputer firmware. USB is the default. Open Settings, tap HID Status, select
Bluetooth, and pair `CodexDeck AMOLED`. BLE advertising stops when USB is
selected; clearing the saved bond requires a separate confirmation.

Status is currently compile-ready. Upload and touch/display field proof still
requires the Waveshare board to be connected.
