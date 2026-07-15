#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct WifiNetworkEntry {
  String ssid;
  int32_t rssi = -127;
  bool secured = true;
};

class DeckNetwork {
 public:
  void begin();
  void update();
  bool hasCredentials() const;
  bool connected() const;
  const String& ssid() const;
  const String& bridgeHost() const;
  uint16_t bridgePort() const;
  uint32_t lastBridgeSeenMs() const;
  void connectTo(const String& ssid, const String& password);
  void retry();
  size_t scan();
  size_t networkCount() const;
  const WifiNetworkEntry* networkAt(size_t index) const;

 private:
  static constexpr size_t MAX_NETWORKS = 12;
  Preferences preferences_;
  String savedSsid_;
  String savedPassword_;
  String bridgeHost_;
  uint16_t bridgePort_ = 8765;
  uint32_t lastBridgeSeenMs_ = 0;
  uint32_t lastWifiAttemptMs_ = 0;
  uint32_t lastDiscoveryMs_ = 0;
  bool mdnsStarted_ = false;
  WifiNetworkEntry networks_[MAX_NETWORKS];
  size_t networkCount_ = 0;

  void startWifi();
  void discoverBridge();
  void saveBridgeHint(const String& host, uint16_t port);
};
