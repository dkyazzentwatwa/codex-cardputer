#pragma once

#include <Arduino.h>
#include <ControlDeckCore.h>
#include <WebSocketsClient.h>

struct ApprovalState {
  bool open = false;
  char id[129] = {};
  char taskId[129] = {};
  char type[20] = {};
  char title[33] = {};
  char summary[161] = {};
  char command[257] = {};
  char cwd[65] = {};
  char risk[8] = {};
  char riskReason[97] = {};
};

struct UsageState {
  bool received = false;
  bool available = false;
  char limitName[33] = {};
  int16_t primaryRemainingPercent = -1;
  uint32_t primaryWindowMinutes = 0;
  int16_t secondaryRemainingPercent = -1;
  uint32_t secondaryWindowMinutes = 0;
};

class ControlDeckClient {
 public:
  void begin(const String& host, uint16_t port);
  void update(bool wifiConnected);
  void retryNow();
  bool connected() const;
  const String& host() const;
  uint16_t port() const;
  uint32_t lastSeenMs() const;
  uint32_t revision() const;
  codexdeck::TaskStore& tasks();
  const codexdeck::TaskStore& tasks() const;
  const codexdeck::MacroState* globalMacro(size_t index) const;
  size_t globalMacroCount() const;
  const ApprovalState& approval() const;
  const UsageState& usage() const;
  const char* toast() const;
  void sendSelect(const char* taskId);
  void sendStop(const char* taskId);
  void sendFollowup(const char* taskId, const String& prompt);
  void sendWorkflow(const char* macroId);
  void sendSkill(const char* macroId);
  void sendApproval(const char* approvalId, const char* decision);
  void sendClearFinished();
  void requestSnapshot();

 private:
  WebSocketsClient socket_;
  String host_;
  uint16_t port_ = 8765;
  bool transportConnected_ = false;
  bool welcomed_ = false;
  uint8_t reconnectAttempt_ = 0;
  uint32_t nextReconnectAt_ = 0;
  uint32_t lastSeenMs_ = 0;
  uint32_t revision_ = 0;
  uint32_t requestCounter_ = 0;
  codexdeck::TaskStore tasks_;
  codexdeck::MacroState globalMacros_[codexdeck::MAX_GLOBAL_MACROS] = {};
  size_t globalMacroCount_ = 0;
  ApprovalState approval_;
  UsageState usage_;
  char toast_[97] = {};
  uint32_t toastExpiresAt_ = 0;

  void connectSocket();
  void onSocketEvent(WStype_t type, uint8_t* payload, size_t length);
  void handleMessage(const uint8_t* payload, size_t length);
  void sendHello();
  void sendJson(String payload);
  String requestId();
};
