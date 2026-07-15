#include <ControlDeckCore.h>

using namespace codexdeck;

uint16_t passed = 0;
uint16_t failed = 0;

void check(bool condition, const char* name);
TaskState makeTask(const char* id, TaskStatus status, bool attention, const char* updated);

void check(bool condition, const char* name) {
  if (condition) {
    ++passed;
    Serial.printf("PASS %s\n", name);
  } else {
    ++failed;
    Serial.printf("FAIL %s\n", name);
  }
}

TaskState makeTask(const char* id, TaskStatus status, bool attention, const char* updated) {
  TaskState value;
  copyText(value.id, sizeof(value.id), id);
  copyText(value.title, sizeof(value.title), id);
  copyText(value.updatedAt, sizeof(value.updatedAt), updated);
  value.status = status;
  value.requiresAttention = attention;
  return value;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  TaskStore store;
  store.upsert(makeTask("run", TaskStatus::Running, false, "2026-07-15T10:00:00Z"));
  store.upsert(makeTask("wait", TaskStatus::WaitingApproval, true, "2026-07-15T09:00:00Z"));
  check(store.count() == 2, "task count");
  check(strcmp(store.at(0)->id, "wait") == 0, "attention sorting");
  store.select(1);
  store.upsert(makeTask("run", TaskStatus::Running, false, "2026-07-15T11:00:00Z"));
  check(strcmp(store.selected()->id, "run") == 0, "selection retention");
  check(store.selectById("wait") && strcmp(store.selected()->id, "wait") == 0, "selection by task id");
  store.markAllStale();
  check(store.find("run")->status == TaskStatus::Stale, "offline stale marking");

  ApprovalController approval;
  check(!approval.press(1000), "first approval press arms");
  check(approval.press(2000), "second approval press confirms");
  check(!approval.press(3000), "approval rearms after submit");
  check(!approval.press(8001), "approval timeout blocks late press");
  check(approval.press(8002), "approval confirms after timeout rearm");
  approval.beginHold(3000);
  check(!approval.updateHold(4499), "early hold blocked");
  check(approval.updateHold(4500), "full hold confirms");
  approval.beginHold(5000);
  approval.cancelHold();
  check(!approval.updateHold(7000), "released hold does not confirm");

  check(reconnectDelayMs(0, 0) == 500, "initial reconnect delay");
  check(reconnectDelayMs(8, 0) == 30000, "reconnect delay cap");
  check(parseStatus("waiting_input") == TaskStatus::WaitingInput, "status parsing");
  check(!mutationAllowed(false), "offline mutation blocking");
  check(!mutationAllowed(true, false), "disabled macro blocking");
  check(mutationAllowed(true), "online mutation allowed");

  char bounded[5];
  copyText(bounded, sizeof(bounded), "123456789");
  check(strcmp(bounded, "1234") == 0, "bounded text truncation");

  Serial.printf("TEST SUMMARY passed=%u failed=%u\n", passed, failed);
}

void loop() { delay(1000); }
