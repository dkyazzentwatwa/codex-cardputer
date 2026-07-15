#include "control_deck_client.h"

#include <ArduinoJson.h>
#include <ESP.h>

#include <stddef.h>
#include <string.h>

namespace {
constexpr size_t MAX_FRAME_BYTES = 8192;
constexpr size_t JSON_ARENA_BYTES = 32768;
constexpr uint32_t STALE_AFTER_MS = 30000;

class BoundedJsonAllocator : public ArduinoJson::Allocator {
 public:
  void reset() { used_ = 0; }

  void* allocate(size_t size) override {
    const size_t aligned = (used_ + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);
    const size_t required = aligned + sizeof(Header) + size;
    if (required > sizeof(arena_)) return nullptr;
    Header* header = reinterpret_cast<Header*>(arena_ + aligned);
    header->size = size;
    used_ = required;
    return header + 1;
  }

  void deallocate(void*) override {}

  void* reallocate(void* pointer, size_t newSize) override {
    if (!pointer) return allocate(newSize);
    Header* header = static_cast<Header*>(pointer) - 1;
    const size_t oldSize = header->size;
    const size_t blockEnd = static_cast<size_t>(reinterpret_cast<uint8_t*>(pointer) - arena_) + oldSize;
    if (blockEnd == used_) {
      const size_t nextEnd = static_cast<size_t>(reinterpret_cast<uint8_t*>(pointer) - arena_) + newSize;
      if (nextEnd > sizeof(arena_)) return nullptr;
      header->size = newSize;
      used_ = nextEnd;
      return pointer;
    }
    void* replacement = allocate(newSize);
    if (replacement) memcpy(replacement, pointer, min(oldSize, newSize));
    return replacement;
  }

 private:
  struct alignas(max_align_t) Header {
    size_t size;
  };
  alignas(max_align_t) uint8_t arena_[JSON_ARENA_BYTES] = {};
  size_t used_ = 0;
};

BoundedJsonAllocator inboundAllocator;

void parseMacro(JsonObjectConst source, codexdeck::MacroState& target) {
  codexdeck::copyText(target.id, sizeof(target.id), source["id"] | "");
  codexdeck::copyText(target.label, sizeof(target.label), source["label"] | "");
  codexdeck::copyText(target.shortLabel, sizeof(target.shortLabel), source["shortLabel"] | "");
  codexdeck::copyText(target.action, sizeof(target.action), source["action"] | "");
  codexdeck::copyText(target.confirmation, sizeof(target.confirmation), source["confirmation"] | "none");
  target.enabled = source["enabled"] | false;
}

void parseTask(JsonObjectConst source, codexdeck::TaskState& target) {
  codexdeck::copyText(target.id, sizeof(target.id), source["id"] | "");
  codexdeck::copyText(target.threadId, sizeof(target.threadId), source["threadId"] | "");
  codexdeck::copyText(target.turnId, sizeof(target.turnId), source["turnId"] | "");
  codexdeck::copyText(target.projectId, sizeof(target.projectId), source["projectId"] | "");
  codexdeck::copyText(target.title, sizeof(target.title), source["title"] | "Untitled");
  codexdeck::copyText(target.summary, sizeof(target.summary), source["summary"] | "Working on task");
  codexdeck::copyText(target.detail, sizeof(target.detail), source["detail"] | "");
  codexdeck::copyText(target.updatedAt, sizeof(target.updatedAt), source["updatedAt"] | "");
  codexdeck::copyText(target.pendingApprovalId, sizeof(target.pendingApprovalId), source["pendingApprovalId"] | "");
  target.status = codexdeck::parseStatus(source["status"] | "starting");
  target.elapsedSeconds = source["elapsedSeconds"] | 0;
  target.requiresAttention = source["requiresAttention"] | false;
  target.macroCount = 0;
  JsonArrayConst macros = source["macros"].as<JsonArrayConst>();
  for (JsonObjectConst macro : macros) {
    if (target.macroCount >= codexdeck::MAX_TASK_MACROS) break;
    parseMacro(macro, target.macros[target.macroCount++]);
  }
}
}  // namespace

void ControlDeckClient::begin(const String& host, uint16_t port) {
  if (host.length() == 0) return;
  if (host_ != host || port_ != port) {
    socket_.disconnect();
    transportConnected_ = false;
    welcomed_ = false;
  }
  host_ = host;
  port_ = port;
  reconnectAttempt_ = 0;
  nextReconnectAt_ = 0;
  connectSocket();
}

void ControlDeckClient::update(bool wifiConnected) {
  if (!wifiConnected || host_.length() == 0) return;
  socket_.loop();
  if (welcomed_ && lastSeenMs_ && millis() - lastSeenMs_ > STALE_AFTER_MS) {
    codexdeck::copyText(toast_, sizeof(toast_), "Bridge heartbeat timed out");
    retryNow();
    return;
  }
  if (!transportConnected_ && static_cast<int32_t>(millis() - nextReconnectAt_) >= 0) connectSocket();
}

void ControlDeckClient::retryNow() {
  socket_.disconnect();
  transportConnected_ = false;
  welcomed_ = false;
  nextReconnectAt_ = 0;
}

bool ControlDeckClient::connected() const { return welcomed_; }
const String& ControlDeckClient::host() const { return host_; }
uint16_t ControlDeckClient::port() const { return port_; }
uint32_t ControlDeckClient::lastSeenMs() const { return lastSeenMs_; }
uint32_t ControlDeckClient::revision() const { return revision_; }
codexdeck::TaskStore& ControlDeckClient::tasks() { return tasks_; }
const codexdeck::TaskStore& ControlDeckClient::tasks() const { return tasks_; }

const codexdeck::MacroState* ControlDeckClient::globalMacro(size_t index) const {
  return index < globalMacroCount_ ? &globalMacros_[index] : nullptr;
}

size_t ControlDeckClient::globalMacroCount() const { return globalMacroCount_; }
const ApprovalState& ControlDeckClient::approval() const { return approval_; }
const char* ControlDeckClient::toast() const { return toast_; }

void ControlDeckClient::sendSelect(const char* taskId) {
  JsonDocument document;
  document["type"] = "task.select";
  document["taskId"] = taskId;
  String payload;
  serializeJson(document, payload);
  sendJson(payload);
}

void ControlDeckClient::sendStop(const char* taskId) {
  JsonDocument document;
  document["type"] = "task.stop.request";
  document["requestId"] = requestId();
  document["taskId"] = taskId;
  String payload;
  serializeJson(document, payload);
  sendJson(payload);
}

void ControlDeckClient::sendFollowup(const char* taskId, const String& prompt) {
  if (prompt.length() == 0 || prompt.length() > 240) return;
  JsonDocument document;
  document["type"] = "task.followup.submit";
  document["requestId"] = requestId();
  document["taskId"] = taskId;
  document["prompt"] = prompt;
  String payload;
  serializeJson(document, payload);
  sendJson(payload);
}

void ControlDeckClient::sendWorkflow(const char* macroId) {
  String id(macroId);
  const int separator = id.indexOf('/');
  if (separator <= 0) return;
  JsonDocument document;
  document["type"] = "workflow.launch.request";
  document["requestId"] = requestId();
  document["projectId"] = id.substring(0, separator);
  document["workflowId"] = id.substring(separator + 1);
  String payload;
  serializeJson(document, payload);
  sendJson(payload);
}

void ControlDeckClient::sendSkill(const char* macroId) {
  String id(macroId);
  if (id.startsWith("skill:")) id.remove(0, 6);
  const int separator = id.indexOf('/');
  if (separator <= 0) return;
  JsonDocument document;
  document["type"] = "skill.launch.request";
  document["requestId"] = requestId();
  document["projectId"] = id.substring(0, separator);
  document["skillName"] = id.substring(separator + 1);
  String payload;
  serializeJson(document, payload);
  sendJson(payload);
}

void ControlDeckClient::sendApproval(const char* approvalId, const char* decision) {
  JsonDocument document;
  document["type"] = "approval.respond";
  document["requestId"] = requestId();
  document["approvalId"] = approvalId;
  document["decision"] = decision;
  String payload;
  serializeJson(document, payload);
  sendJson(payload);
}

void ControlDeckClient::requestSnapshot() {
  sendJson("{\"type\":\"snapshot.request\"}");
}

void ControlDeckClient::connectSocket() {
  if (transportConnected_ || host_.length() == 0) return;
  socket_.disconnect();
  socket_.begin(host_.c_str(), port_, "/device");
  socket_.setReconnectInterval(0);
  socket_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) { onSocketEvent(type, payload, length); });
  nextReconnectAt_ = millis() + codexdeck::reconnectDelayMs(reconnectAttempt_++, esp_random());
  Serial.printf("[diag] action=bridge_connect status=start app=control-deck host=%s port=%u\n", host_.c_str(), port_);
}

void ControlDeckClient::onSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    transportConnected_ = true;
    reconnectAttempt_ = 0;
    sendHello();
  } else if (type == WStype_DISCONNECTED) {
    transportConnected_ = false;
    welcomed_ = false;
    tasks_.markAllStale();
    nextReconnectAt_ = millis() + codexdeck::reconnectDelayMs(reconnectAttempt_++, esp_random());
    ++revision_;
  } else if (type == WStype_TEXT) {
    handleMessage(payload, length);
  }
}

void ControlDeckClient::handleMessage(const uint8_t* payload, size_t length) {
  if (length == 0 || length > MAX_FRAME_BYTES) return;
  inboundAllocator.reset();
  JsonDocument document(&inboundAllocator);
  DeserializationError error = deserializeJson(document, payload, length);
  if (error) {
    Serial.printf("[diag] action=protocol_decode status=fail app=control-deck error=%s\n", error.c_str());
    return;
  }
  const char* type = document["type"] | "";
  lastSeenMs_ = millis();
  if (strcmp(type, "welcome") == 0) {
    welcomed_ = strcmp(document["protocol"] | "", "codexdeck.v1") == 0;
  } else if (strcmp(type, "task.snapshot") == 0) {
    tasks_.clear();
    JsonArrayConst tasks = document["tasks"].as<JsonArrayConst>();
    size_t count = 0;
    for (JsonObjectConst source : tasks) {
      if (count++ >= codexdeck::MAX_TASKS) break;
      codexdeck::TaskState task;
      parseTask(source, task);
      tasks_.upsert(task);
    }
  } else if (strcmp(type, "task.upsert") == 0) {
    codexdeck::TaskState task;
    parseTask(document["task"].as<JsonObjectConst>(), task);
    tasks_.upsert(task);
  } else if (strcmp(type, "task.remove") == 0) {
    tasks_.remove(document["taskId"] | "");
  } else if (strcmp(type, "macro.snapshot") == 0) {
    globalMacroCount_ = 0;
    for (JsonObjectConst source : document["macros"].as<JsonArrayConst>()) {
      if (globalMacroCount_ >= codexdeck::MAX_GLOBAL_MACROS) break;
      parseMacro(source, globalMacros_[globalMacroCount_++]);
    }
  } else if (strcmp(type, "approval.open") == 0) {
    JsonObjectConst source = document["approval"].as<JsonObjectConst>();
    approval_.open = true;
    codexdeck::copyText(approval_.id, sizeof(approval_.id), source["id"] | "");
    codexdeck::copyText(approval_.taskId, sizeof(approval_.taskId), source["taskId"] | "");
    codexdeck::copyText(approval_.type, sizeof(approval_.type), source["type"] | "");
    codexdeck::copyText(approval_.title, sizeof(approval_.title), source["title"] | "Approval");
    codexdeck::copyText(approval_.summary, sizeof(approval_.summary), source["summary"] | "");
    codexdeck::copyText(approval_.command, sizeof(approval_.command), source["command"] | "");
    codexdeck::copyText(approval_.cwd, sizeof(approval_.cwd), source["cwd"] | "");
    codexdeck::copyText(approval_.risk, sizeof(approval_.risk), source["risk"] | "low");
    codexdeck::copyText(approval_.riskReason, sizeof(approval_.riskReason), source["riskReason"] | "");
  } else if (strcmp(type, "approval.resolved") == 0) {
    if (strcmp(approval_.id, document["approvalId"] | "") == 0) approval_.open = false;
  } else if (strcmp(type, "toast") == 0 || strcmp(type, "error") == 0) {
    codexdeck::copyText(toast_, sizeof(toast_), document["message"] | "Bridge message");
  } else if (strcmp(type, "ping") == 0) {
    JsonDocument response;
    response["type"] = "pong";
    response["nonce"] = document["nonce"] | "ping";
    String serialized;
    serializeJson(response, serialized);
    sendJson(serialized);
  }
  ++revision_;
}

void ControlDeckClient::sendHello() {
  char deviceId[32];
  snprintf(deviceId, sizeof(deviceId), "cardputer-%06llx", ESP.getEfuseMac() & 0xFFFFFFULL);
  JsonDocument document;
  document["type"] = "hello";
  document["protocol"] = "codexdeck.v1";
  document["deviceId"] = deviceId;
  document["deviceName"] = "CardPuter ADV";
  document["firmwareVersion"] = "0.1.0";
  JsonArray capabilities = document["capabilities"].to<JsonArray>();
  capabilities.add("keyboard");
  capabilities.add("display");
  capabilities.add("hold-confirm");
  String payload;
  serializeJson(document, payload);
  sendJson(payload);
}

void ControlDeckClient::sendJson(String payload) {
  if (!transportConnected_ || payload.length() > MAX_FRAME_BYTES) return;
  socket_.sendTXT(payload);
}

String ControlDeckClient::requestId() {
  return String("req-") + String(millis(), HEX) + "-" + String(++requestCounter_, HEX);
}
