#pragma once

#include <stddef.h>
#include <stdint.h>

namespace codexdeck {

constexpr size_t MAX_TASKS = 20;
constexpr size_t MAX_TASK_MACROS = 12;
constexpr size_t MAX_GLOBAL_MACROS = 20;
constexpr size_t KEYBOARD_SHORTCUT_PAGE_SIZE = 8;

enum class DeckTheme : uint8_t {
  NeonGrid,
  Terminal,
  AmberCrt,
  Synthwave,
  Ice,
};

size_t deckThemeCount();
DeckTheme deckThemeAt(size_t index);
const char* deckThemeLabel(DeckTheme theme);
bool deckThemeValid(DeckTheme theme);

enum class HidTransport : uint8_t {
  Usb,
  Bluetooth,
};

bool hidTransportValid(HidTransport transport);
HidTransport hidTransportFromStored(uint8_t stored);
const char* hidTransportLabel(HidTransport transport);
bool hidTransportActive(HidTransport selected, HidTransport candidate);

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

enum KeyboardShortcutModifier : uint8_t {
  KeyboardModifierNone = 0,
  KeyboardModifierControl = 1 << 0,
  KeyboardModifierShift = 1 << 1,
  KeyboardModifierAlt = 1 << 2,
  KeyboardModifierGui = 1 << 3,
};

enum class KeyboardShortcutKey : uint8_t {
  Escape,
  Tab,
  LeftBracket,
  RightBracket,
  Comma,
  D,
  Slash,
  Backtick,
  A,
  B,
  F,
  G,
  J,
  L,
  N,
  O,
  P,
  S,
  T,
};

struct KeyboardShortcut {
  const char* id;
  const char* label;
  const char* combo;
  uint8_t page;
  uint8_t modifiers;
  KeyboardShortcutKey key;
};

size_t keyboardShortcutCount();
size_t keyboardShortcutPageCount();
const char* keyboardShortcutPageLabel(size_t page);
size_t keyboardShortcutPageStart(size_t page);
size_t keyboardShortcutPageItemCount(size_t page);
const KeyboardShortcut* keyboardShortcutAt(size_t index);
bool keyboardShortcutValid(const KeyboardShortcut& shortcut);

struct TaskState {
  char id[129] = {};
  char threadId[129] = {};
  char turnId[129] = {};
  char projectId[65] = {};
  char title[29] = {};
  char summary[65] = {};
  char detail[769] = {};
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
  size_t clearableCount() const;
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
