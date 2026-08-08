#pragma once

#include <Arduino.h>
#include <ControlDeckCore.h>

class HidKeyboardController {
 public:
  void begin(codexdeck::HidTransport initialTransport = codexdeck::HidTransport::Usb);
  bool update();
  bool available() const;
  bool ready() const;
  bool send(const codexdeck::KeyboardShortcut& shortcut);
  bool setTransport(codexdeck::HidTransport transport);
  bool clearBluetoothBonds();
  bool bluetoothBonded() const;
  bool bluetoothStateKnown() const;
  codexdeck::HidTransport transport() const;
  const char* status() const;

 private:
  codexdeck::HidTransport transport_ = codexdeck::HidTransport::Usb;
  bool usbAvailable_ = false;
  bool usbReady_ = false;
  bool bluetoothAvailable_ = false;
  bool bluetoothInitialized_ = false;
  bool bluetoothRunning_ = false;
  bool bluetoothBonded_ = false;
  bool ready_ = false;
  const char* status_ = "HID unavailable";

  void startBluetooth();
  void stopBluetooth();
  void refreshState();
};
