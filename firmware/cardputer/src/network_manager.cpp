#include "network_manager.h"

#include <ESPmDNS.h>
#include <WiFi.h>

namespace {
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t DISCOVERY_INTERVAL_MS = 5000;
}

void DeckNetwork::begin() {
  preferences_.begin("codexdeck", false);
  savedSsid_ = preferences_.getString("wifi_ssid", "");
  savedPassword_ = preferences_.getString("wifi_pass", "");
  bridgeHost_ = preferences_.getString("bridge_host", "");
  bridgePort_ = preferences_.getUShort("bridge_port", 8765);
  if (hasCredentials()) startWifi();
}

void DeckNetwork::update() {
  if (!hasCredentials()) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiAttemptMs_ >= WIFI_RETRY_MS) startWifi();
    return;
  }
  if (!mdnsStarted_) {
    mdnsStarted_ = MDNS.begin("cardputer-codex");
    Serial.printf("[diag] action=mdns_start status=%s app=control-deck\n", mdnsStarted_ ? "ok" : "fail");
  }
  if (millis() - lastDiscoveryMs_ >= DISCOVERY_INTERVAL_MS) discoverBridge();
}

bool DeckNetwork::hasCredentials() const { return savedSsid_.length() > 0; }
bool DeckNetwork::connected() const { return WiFi.status() == WL_CONNECTED; }
const String& DeckNetwork::ssid() const { return savedSsid_; }
const String& DeckNetwork::bridgeHost() const { return bridgeHost_; }
uint16_t DeckNetwork::bridgePort() const { return bridgePort_; }
uint32_t DeckNetwork::lastBridgeSeenMs() const { return lastBridgeSeenMs_; }

void DeckNetwork::connectTo(const String& ssid, const String& password) {
  savedSsid_ = ssid;
  savedPassword_ = password;
  preferences_.putString("wifi_ssid", savedSsid_);
  preferences_.putString("wifi_pass", savedPassword_);
  startWifi();
}

void DeckNetwork::retry() {
  if (hasCredentials()) startWifi();
  if (connected()) discoverBridge();
}

size_t DeckNetwork::scan() {
  WiFi.mode(WIFI_STA);
  const int found = WiFi.scanNetworks(false, true);
  networkCount_ = 0;
  for (int index = 0; index < found && networkCount_ < MAX_NETWORKS; ++index) {
    const String candidate = WiFi.SSID(index);
    bool duplicate = false;
    for (size_t existing = 0; existing < networkCount_; ++existing) {
      if (networks_[existing].ssid == candidate) duplicate = true;
    }
    if (duplicate || candidate.length() == 0) continue;
    networks_[networkCount_].ssid = candidate;
    networks_[networkCount_].rssi = WiFi.RSSI(index);
    networks_[networkCount_].secured = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    ++networkCount_;
  }
  WiFi.scanDelete();
  Serial.printf("[diag] action=wifi_scan status=ok app=control-deck networks=%u\n", networkCount_);
  return networkCount_;
}

size_t DeckNetwork::networkCount() const { return networkCount_; }

const WifiNetworkEntry* DeckNetwork::networkAt(size_t index) const {
  return index < networkCount_ ? &networks_[index] : nullptr;
}

void DeckNetwork::startWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(savedSsid_.c_str(), savedPassword_.c_str());
  lastWifiAttemptMs_ = millis();
  mdnsStarted_ = false;
  Serial.printf("[diag] action=wifi_connect status=start app=control-deck ssid_len=%u\n", savedSsid_.length());
}

void DeckNetwork::discoverBridge() {
  lastDiscoveryMs_ = millis();
  const int found = MDNS.queryService("codexdeck", "tcp");
  if (found <= 0) return;
  const String host = MDNS.address(0).toString();
  const uint16_t port = MDNS.port(0);
  lastBridgeSeenMs_ = millis();
  if (host != bridgeHost_ || port != bridgePort_) saveBridgeHint(host, port);
  Serial.printf("[diag] action=bridge_discovery status=ok app=control-deck host=%s port=%u\n", host.c_str(), port);
}

void DeckNetwork::saveBridgeHint(const String& host, uint16_t port) {
  bridgeHost_ = host;
  bridgePort_ = port;
  preferences_.putString("bridge_host", host);
  preferences_.putUShort("bridge_port", port);
}
