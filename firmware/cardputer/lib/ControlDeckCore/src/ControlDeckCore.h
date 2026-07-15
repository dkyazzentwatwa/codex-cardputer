#pragma once

#include <stddef.h>
#include <stdint.h>

namespace codexdeck {

constexpr size_t MAX_TASKS = 20;
constexpr size_t MAX_TASK_MACROS = 12;
constexpr size_t MAX_GLOBAL_MACROS = 20;

enum class TaskStatus : uint8_t {
  Starting,
  Running,
  WaitingApproval,
  WaitingInput,
  Completed,
  Failed,
  Cancelled,
  Stale,
};

struct MacroState {
  char id[129] = {};
  char label[49] = {};
  char shortLabel[17] = {};
  char action[25] = {};
  char confirmation[12] = {};
  bool enabled = false;
};

struct TaskState {
  char id[129] = {};
  char threadId[129] = {};
  char turnId[129] = {};
  char projectId[65] = {};
  char title[29] = {};
  char summary[65] = {};
  char detail[161] = {};
  char updatedAt[40] = {};
  char pendingApprovalId[129] = {};
  TaskStatus status = TaskStatus::Starting;
  uint32_t elapsedSeconds = 0;
  bool requiresAttention = false;
  MacroState macros[MAX_TASK_MACROS] = {};
  size_t macroCount = 0;
};

void copyText(char* destination, size_t capacity, const char* source);
const char* statusLabel(TaskStatus status);
TaskStatus parseStatus(const char* value);
uint32_t reconnectDelayMs(uint8_t attempt, uint32_t jitter);
bool mutationAllowed(bool connected, bool actionEnabled = true);

class TaskStore {
 public:
  void clear();
  bool upsert(const TaskState& task);
  bool remove(const char* taskId);
  void sort();
  size_t count() const;
  const TaskState* at(size_t index) const;
  TaskState* find(const char* taskId);
  const TaskState* selected() const;
  size_t selectedIndex() const;
  void select(size_t index);
  bool selectById(const char* taskId);
  void moveSelection(int delta);
  void selectNextAttention();
  void markAllStale();

 private:
  TaskState tasks_[MAX_TASKS] = {};
  size_t count_ = 0;
  size_t selected_ = 0;
};

class ApprovalController {
 public:
  void reset();
  bool press(uint32_t nowMs);
  void beginHold(uint32_t nowMs);
  void cancelHold();
  bool updateHold(uint32_t nowMs);
  bool armed(uint32_t nowMs);
  uint8_t holdPercent(uint32_t nowMs) const;

 private:
  bool armed_ = false;
  uint32_t armedAt_ = 0;
  bool holding_ = false;
  uint32_t holdStartedAt_ = 0;
};

}  // namespace codexdeck
