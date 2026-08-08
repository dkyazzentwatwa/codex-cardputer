#include "ControlDeckCore.h"

#include <string.h>

namespace codexdeck {
namespace {
constexpr uint32_t ARM_TIMEOUT_MS = 5000;
constexpr uint32_t HOLD_DURATION_MS = 1500;
constexpr uint8_t KEYBOARD_SHORTCUT_MODIFIER_MASK = KeyboardModifierControl | KeyboardModifierShift |
                                                   KeyboardModifierAlt | KeyboardModifierGui;
constexpr const char* kKeyboardShortcutPageLabels[] = {"TASKS", "NAV", "PANELS"};
constexpr DeckTheme kDeckThemes[] = {
    DeckTheme::NeonGrid,
    DeckTheme::Terminal,
    DeckTheme::AmberCrt,
    DeckTheme::Synthwave,
    DeckTheme::Ice,
};
constexpr const char* kDeckThemeLabels[] = {"NEON GRID", "TERMINAL", "AMBER CRT", "SYNTHWAVE", "ICE"};

constexpr KeyboardShortcut kKeyboardShortcuts[] = {
    {"escape", "Escape", "Esc", 0, KeyboardModifierNone, KeyboardShortcutKey::Escape},
    {"new-task", "New Task", "Cmd+N", 0, KeyboardModifierGui, KeyboardShortcutKey::N},
    {"new-projectless-task", "Projectless Task", "Cmd+Sh+O", 0, KeyboardModifierGui | KeyboardModifierShift,
     KeyboardShortcutKey::O},
    {"quick-chat", "Quick Chat", "Cmd+Op+N", 0, KeyboardModifierGui | KeyboardModifierAlt,
     KeyboardShortcutKey::N},
    {"archive-task", "Archive Task", "Cmd+Sh+A", 0, KeyboardModifierGui | KeyboardModifierShift,
     KeyboardShortcutKey::A},
    {"toggle-pin", "Pin / Unpin", "Cmd+Op+P", 0, KeyboardModifierGui | KeyboardModifierAlt,
     KeyboardShortcutKey::P},
    {"open-side-task", "Open Side Task", "Cmd+Op+S", 0, KeyboardModifierGui | KeyboardModifierAlt,
     KeyboardShortcutKey::S},
    {"voice-input", "Voice Input", "Ctl+Sh+D", 0, KeyboardModifierControl | KeyboardModifierShift,
     KeyboardShortcutKey::D},
    {"previous-task", "Previous Task", "Cmd+Sh+[", 1, KeyboardModifierGui | KeyboardModifierShift,
     KeyboardShortcutKey::LeftBracket},
    {"next-task", "Next Task", "Cmd+Sh+]", 1, KeyboardModifierGui | KeyboardModifierShift,
     KeyboardShortcutKey::RightBracket},
    {"previous-recent-task", "Previous Recent", "Ctl+Sh+Tab", 1, KeyboardModifierControl | KeyboardModifierShift,
     KeyboardShortcutKey::Tab},
    {"next-recent-task", "Next Recent", "Ctl+Tab", 1, KeyboardModifierControl, KeyboardShortcutKey::Tab},
    {"back", "Back", "Cmd+[", 1, KeyboardModifierGui, KeyboardShortcutKey::LeftBracket},
    {"forward", "Forward", "Cmd+]", 1, KeyboardModifierGui, KeyboardShortcutKey::RightBracket},
    {"find", "Find", "Cmd+F", 1, KeyboardModifierGui, KeyboardShortcutKey::F},
    {"focus-browser-address", "Focus Browser", "Cmd+L", 1, KeyboardModifierGui, KeyboardShortcutKey::L},
    {"open-browser-tab", "Open Browser Tab", "Cmd+T", 2, KeyboardModifierGui, KeyboardShortcutKey::T},
    {"open-review-tab", "Open Review Tab", "Ctl+Sh+G", 2, KeyboardModifierControl | KeyboardModifierShift,
     KeyboardShortcutKey::G},
    {"toggle-bottom-panel", "Toggle Bottom", "Cmd+J", 2, KeyboardModifierGui, KeyboardShortcutKey::J},
    {"toggle-browser-panel", "Toggle Browser", "Cmd+Sh+B", 2, KeyboardModifierGui | KeyboardModifierShift,
     KeyboardShortcutKey::B},
    {"toggle-side-panel", "Toggle Side", "Cmd+Op+B", 2, KeyboardModifierGui | KeyboardModifierAlt,
     KeyboardShortcutKey::B},
    {"open-terminal", "Open Terminal", "Ctl+`", 2, KeyboardModifierControl, KeyboardShortcutKey::Backtick},
    {"settings", "Settings", "Cmd+,", 2, KeyboardModifierGui, KeyboardShortcutKey::Comma},
    {"keyboard-shortcuts", "Keyboard Shortcuts", "Cmd+Sh+/", 2, KeyboardModifierGui | KeyboardModifierShift,
     KeyboardShortcutKey::Slash},
};

constexpr size_t kKeyboardShortcutCount = sizeof(kKeyboardShortcuts) / sizeof(kKeyboardShortcuts[0]);

bool comesBefore(const TaskState& left, const TaskState& right) {
  if (left.requiresAttention != right.requiresAttention) return left.requiresAttention;
  const bool leftRunning = left.status == TaskStatus::Running;
  const bool rightRunning = right.status == TaskStatus::Running;
  if (leftRunning != rightRunning) return leftRunning;
  return strcmp(left.updatedAt, right.updatedAt) > 0;
}

void swapTasks(TaskState& left, TaskState& right) {
  uint8_t buffer[32];
  auto* leftBytes = reinterpret_cast<uint8_t*>(&left);
  auto* rightBytes = reinterpret_cast<uint8_t*>(&right);
  for (size_t offset = 0; offset < sizeof(TaskState); offset += sizeof(buffer)) {
    const size_t remaining = sizeof(TaskState) - offset;
    const size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    memcpy(buffer, leftBytes + offset, chunk);
    memcpy(leftBytes + offset, rightBytes + offset, chunk);
    memcpy(rightBytes + offset, buffer, chunk);
  }
}
}  // namespace

void copyText(char* destination, size_t capacity, const char* source) {
  if (capacity == 0) return;
  if (!source) source = "";
  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

size_t deckThemeCount() { return sizeof(kDeckThemes) / sizeof(kDeckThemes[0]); }

DeckTheme deckThemeAt(size_t index) {
  return index < deckThemeCount() ? kDeckThemes[index] : DeckTheme::NeonGrid;
}

const char* deckThemeLabel(DeckTheme theme) {
  const size_t index = static_cast<size_t>(theme);
  return index < deckThemeCount() ? kDeckThemeLabels[index] : "NEON GRID";
}

bool deckThemeValid(DeckTheme theme) { return static_cast<size_t>(theme) < deckThemeCount(); }

bool hidTransportValid(HidTransport transport) {
  return transport == HidTransport::Usb || transport == HidTransport::Bluetooth;
}

HidTransport hidTransportFromStored(uint8_t stored) {
  const HidTransport transport = static_cast<HidTransport>(stored);
  return hidTransportValid(transport) ? transport : HidTransport::Usb;
}

const char* hidTransportLabel(HidTransport transport) {
  return transport == HidTransport::Bluetooth ? "BLUETOOTH" : "USB";
}

bool hidTransportActive(HidTransport selected, HidTransport candidate) {
  return hidTransportValid(selected) && hidTransportValid(candidate) && selected == candidate;
}

size_t keyboardShortcutCount() { return kKeyboardShortcutCount; }

size_t keyboardShortcutPageCount() {
  return (kKeyboardShortcutCount + KEYBOARD_SHORTCUT_PAGE_SIZE - 1) / KEYBOARD_SHORTCUT_PAGE_SIZE;
}

const char* keyboardShortcutPageLabel(size_t page) {
  return page < keyboardShortcutPageCount() ? kKeyboardShortcutPageLabels[page] : "";
}

size_t keyboardShortcutPageStart(size_t page) {
  return page < keyboardShortcutPageCount() ? page * KEYBOARD_SHORTCUT_PAGE_SIZE : kKeyboardShortcutCount;
}

size_t keyboardShortcutPageItemCount(size_t page) {
  const size_t start = keyboardShortcutPageStart(page);
  const size_t remaining = kKeyboardShortcutCount - start;
  return remaining < KEYBOARD_SHORTCUT_PAGE_SIZE ? remaining : KEYBOARD_SHORTCUT_PAGE_SIZE;
}

const KeyboardShortcut* keyboardShortcutAt(size_t index) {
  return index < kKeyboardShortcutCount ? &kKeyboardShortcuts[index] : nullptr;
}

bool keyboardShortcutValid(const KeyboardShortcut& shortcut) {
  return shortcut.id && shortcut.id[0] && shortcut.label && shortcut.label[0] && shortcut.combo && shortcut.combo[0] &&
         shortcut.page < keyboardShortcutPageCount() && (shortcut.modifiers & ~KEYBOARD_SHORTCUT_MODIFIER_MASK) == 0;
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
  char selectedId[129] = {};
  if (selected_ < count_) copyText(selectedId, sizeof(selectedId), tasks_[selected_].id);
  for (size_t index = 0; index < count_; ++index) {
    if (strcmp(tasks_[index].id, taskId) != 0) continue;
    for (size_t move = index; move + 1 < count_; ++move) tasks_[move] = tasks_[move + 1];
    --count_;
    if (count_ == 0) {
      selected_ = 0;
    } else if (strcmp(selectedId, taskId) == 0) {
      selected_ = index < count_ ? index : count_ - 1;
    } else {
      for (size_t candidate = 0; candidate < count_; ++candidate) {
        if (strcmp(tasks_[candidate].id, selectedId) == 0) {
          selected_ = candidate;
          break;
        }
      }
    }
    return true;
  }
  return false;
}

void TaskStore::sort() {
  char selectedId[129] = {};
  if (selected_ < count_) copyText(selectedId, sizeof(selectedId), tasks_[selected_].id);
  for (size_t index = 1; index < count_; ++index) {
    size_t insert = index;
    while (insert > 0 && comesBefore(tasks_[insert], tasks_[insert - 1])) {
      swapTasks(tasks_[insert], tasks_[insert - 1]);
      --insert;
    }
  }
  for (size_t index = 0; index < count_; ++index) {
    if (strcmp(tasks_[index].id, selectedId) == 0) {
      selected_ = index;
      break;
    }
  }
}

size_t TaskStore::count() const { return count_; }

size_t TaskStore::clearableCount() const {
  size_t clearable = 0;
  for (size_t index = 0; index < count_; ++index) {
    const TaskStatus status = tasks_[index].status;
    if (status == TaskStatus::Completed || status == TaskStatus::Failed || status == TaskStatus::Cancelled) {
      ++clearable;
    }
  }
  return clearable;
}

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
