#include "ControlDeckCore.h"

#include <string.h>

namespace codexdeck {
namespace {
constexpr uint32_t ARM_TIMEOUT_MS = 5000;
constexpr uint32_t HOLD_DURATION_MS = 1500;

bool comesBefore(const TaskState& left, const TaskState& right) {
  if (left.requiresAttention != right.requiresAttention) return left.requiresAttention;
  const bool leftRunning = left.status == TaskStatus::Running;
  const bool rightRunning = right.status == TaskStatus::Running;
  if (leftRunning != rightRunning) return leftRunning;
  return strcmp(left.updatedAt, right.updatedAt) > 0;
}
}  // namespace

void copyText(char* destination, size_t capacity, const char* source) {
  if (capacity == 0) return;
  if (!source) source = "";
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

const char* statusLabel(TaskStatus status) {
  switch (status) {
    case TaskStatus::Starting: return "START";
    case TaskStatus::Running: return "RUN";
    case TaskStatus::WaitingApproval: return "WAIT";
    case TaskStatus::WaitingInput: return "INPUT";
    case TaskStatus::Completed: return "DONE";
    case TaskStatus::Failed: return "FAIL";
    case TaskStatus::Cancelled: return "CANCEL";
    case TaskStatus::Stale: return "STALE";
  }
  return "?";
}

TaskStatus parseStatus(const char* value) {
  if (!value) return TaskStatus::Starting;
  if (strcmp(value, "running") == 0) return TaskStatus::Running;
  if (strcmp(value, "waiting_approval") == 0) return TaskStatus::WaitingApproval;
  if (strcmp(value, "waiting_input") == 0) return TaskStatus::WaitingInput;
  if (strcmp(value, "completed") == 0) return TaskStatus::Completed;
  if (strcmp(value, "failed") == 0) return TaskStatus::Failed;
  if (strcmp(value, "cancelled") == 0) return TaskStatus::Cancelled;
  if (strcmp(value, "stale") == 0) return TaskStatus::Stale;
  return TaskStatus::Starting;
}

uint32_t reconnectDelayMs(uint8_t attempt, uint32_t jitter) {
  const uint8_t capped = attempt > 6 ? 6 : attempt;
  uint32_t delayMs = 500UL << capped;
  if (delayMs > 30000UL) delayMs = 30000UL;
  return delayMs + (jitter % 251UL);
}

bool mutationAllowed(bool connected, bool actionEnabled) {
  return connected && actionEnabled;
}

void TaskStore::clear() {
  memset(tasks_, 0, sizeof(tasks_));
  count_ = 0;
  selected_ = 0;
}

bool TaskStore::upsert(const TaskState& task) {
  for (size_t index = 0; index < count_; ++index) {
    if (strcmp(tasks_[index].id, task.id) == 0) {
      tasks_[index] = task;
      sort();
      return true;
    }
  }
  if (count_ >= MAX_TASKS) return false;
  tasks_[count_++] = task;
  sort();
  return true;
}

bool TaskStore::remove(const char* taskId) {
  for (size_t index = 0; index < count_; ++index) {
    if (strcmp(tasks_[index].id, taskId) != 0) continue;
    for (size_t move = index; move + 1 < count_; ++move) tasks_[move] = tasks_[move + 1];
    --count_;
    if (selected_ >= count_ && count_ > 0) selected_ = count_ - 1;
    return true;
  }
  return false;
}

void TaskStore::sort() {
  char selectedId[129] = {};
  if (selected_ < count_) copyText(selectedId, sizeof(selectedId), tasks_[selected_].id);
  for (size_t index = 1; index < count_; ++index) {
    TaskState value = tasks_[index];
    size_t insert = index;
    while (insert > 0 && comesBefore(value, tasks_[insert - 1])) {
      tasks_[insert] = tasks_[insert - 1];
      --insert;
    }
    tasks_[insert] = value;
  }
  for (size_t index = 0; index < count_; ++index) {
    if (strcmp(tasks_[index].id, selectedId) == 0) {
      selected_ = index;
      break;
    }
  }
}

size_t TaskStore::count() const { return count_; }

const TaskState* TaskStore::at(size_t index) const {
  return index < count_ ? &tasks_[index] : nullptr;
}

TaskState* TaskStore::find(const char* taskId) {
  for (size_t index = 0; index < count_; ++index) {
    if (strcmp(tasks_[index].id, taskId) == 0) return &tasks_[index];
  }
  return nullptr;
}

const TaskState* TaskStore::selected() const { return at(selected_); }
size_t TaskStore::selectedIndex() const { return selected_; }

void TaskStore::select(size_t index) {
  if (index < count_) selected_ = index;
}

bool TaskStore::selectById(const char* taskId) {
  for (size_t index = 0; index < count_; ++index) {
    if (strcmp(tasks_[index].id, taskId) != 0) continue;
    selected_ = index;
    return true;
  }
  return false;
}

void TaskStore::moveSelection(int delta) {
  if (count_ == 0) return;
  int next = static_cast<int>(selected_) + delta;
  while (next < 0) next += static_cast<int>(count_);
  selected_ = static_cast<size_t>(next) % count_;
}

void TaskStore::selectNextAttention() {
  if (count_ == 0) return;
  for (size_t offset = 1; offset <= count_; ++offset) {
    const size_t candidate = (selected_ + offset) % count_;
    if (tasks_[candidate].requiresAttention) {
      selected_ = candidate;
      return;
    }
  }
  moveSelection(1);
}

void TaskStore::markAllStale() {
  for (size_t index = 0; index < count_; ++index) {
    if (tasks_[index].status == TaskStatus::Starting || tasks_[index].status == TaskStatus::Running ||
        tasks_[index].status == TaskStatus::WaitingApproval || tasks_[index].status == TaskStatus::WaitingInput) {
      tasks_[index].status = TaskStatus::Stale;
      tasks_[index].requiresAttention = false;
    }
  }
}

void ApprovalController::reset() {
  armed_ = false;
  armedAt_ = 0;
  holding_ = false;
  holdStartedAt_ = 0;
}

bool ApprovalController::press(uint32_t nowMs) {
  if (armed_ && nowMs - armedAt_ <= ARM_TIMEOUT_MS) {
    reset();
    return true;
  }
  armed_ = true;
  armedAt_ = nowMs;
  return false;
}

void ApprovalController::beginHold(uint32_t nowMs) {
  if (!holding_) {
    holding_ = true;
    holdStartedAt_ = nowMs;
  }
}

void ApprovalController::cancelHold() {
  holding_ = false;
  holdStartedAt_ = 0;
}

bool ApprovalController::updateHold(uint32_t nowMs) {
  if (!holding_ || nowMs - holdStartedAt_ < HOLD_DURATION_MS) return false;
  reset();
  return true;
}

bool ApprovalController::armed(uint32_t nowMs) {
  if (armed_ && nowMs - armedAt_ > ARM_TIMEOUT_MS) armed_ = false;
  return armed_;
}

uint8_t ApprovalController::holdPercent(uint32_t nowMs) const {
  if (!holding_) return 0;
  const uint32_t elapsed = nowMs - holdStartedAt_;
  return elapsed >= HOLD_DURATION_MS ? 100 : static_cast<uint8_t>((elapsed * 100UL) / HOLD_DURATION_MS);
}

}  // namespace codexdeck
