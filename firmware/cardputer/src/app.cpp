#include "app.h"

#include <M5Cardputer.h>

#include "control_deck_client.h"
#include "input_controller.h"
#include "network_manager.h"
#include "ui_renderer.h"

namespace {
enum class Screen : uint8_t {
  Dashboard,
  Detail,
  Macros,
  Approval,
  Followup,
  Offline,
  WifiList,
  WifiSsid,
  WifiPassword,
  ConfirmStop,
  Diagnostics,
  Keymap,
};

DeckNetwork network;
ControlDeckClient client;
InputController input;
UiRenderer ui;
codexdeck::ApprovalController approvalConfirmation;
Screen screen = Screen::Offline;
size_t macroSelected = 0;
size_t macroPage = 0;
size_t taskMacroSelected = 0;
size_t wifiSelected = 0;
String selectedSsid;
String wifiPassword;
String followupText;
String latestKey;
String armedDecision;
String activeBridgeHost;
uint16_t activeBridgePort = 0;
uint32_t seenRevision = 0;
uint32_t lastDrawMs = 0;
bool highRiskHolding = false;

bool hasText(const InputEvent& event, char expected) {
  for (size_t index = 0; index < event.text.length(); ++index) {
    if (tolower(static_cast<unsigned char>(event.text[index])) == tolower(static_cast<unsigned char>(expected))) return true;
  }
  return false;
}

bool hasInput(const InputEvent& event) {
  return event.up || event.down || event.left || event.right || event.enterPressed || event.back || event.erase ||
         event.tab || event.text.length() > 0;
}

void appendPrintable(String& target, const String& inputText, size_t limit) {
  for (size_t index = 0; index < inputText.length() && target.length() < limit; ++index) {
    const char character = inputText[index];
    if (character >= 32 && character <= 126 && character != '`') target += character;
  }
}

void eraseLast(String& value) {
  if (value.length()) value.remove(value.length() - 1);
}

void openWifiList() {
  network.scan();
  wifiSelected = 0;
  screen = Screen::WifiList;
}

void connectKnownBridge() {
  if (!network.connected() || network.bridgeHost().length() == 0) return;
  if (network.bridgeHost() == activeBridgeHost && network.bridgePort() == activeBridgePort) return;
  activeBridgeHost = network.bridgeHost();
  activeBridgePort = network.bridgePort();
  client.begin(activeBridgeHost, activeBridgePort);
}

void submitApproval(const char* decision) {
  if (!codexdeck::mutationAllowed(client.connected(), client.approval().open)) return;
  client.sendApproval(client.approval().id, decision);
  approvalConfirmation.reset();
  armedDecision = "";
  highRiskHolding = false;
  screen = Screen::Detail;
}

void confirmNormalDecision(const char* decision, uint32_t nowMs) {
  if (armedDecision != decision) {
    approvalConfirmation.reset();
    armedDecision = decision;
  }
  if (approvalConfirmation.press(nowMs)) submitApproval(decision);
}

void launchSelectedMacro() {
  if (!codexdeck::mutationAllowed(client.connected()) || macroSelected >= client.globalMacroCount()) return;
  const codexdeck::MacroState* macro = client.globalMacro(macroSelected);
  if (!macro || !codexdeck::mutationAllowed(client.connected(), macro->enabled)) return;
  if (strcmp(macro->action, "launch_skill") == 0) client.sendSkill(macro->id);
  else client.sendWorkflow(macro->id);
  screen = Screen::Dashboard;
}

void activateTaskMacro(const codexdeck::TaskState& task) {
  if (!codexdeck::mutationAllowed(client.connected()) || task.macroCount == 0) return;
  const codexdeck::MacroState& macro = task.macros[min(taskMacroSelected, task.macroCount - 1)];
  if (!codexdeck::mutationAllowed(client.connected(), macro.enabled)) return;
  if (strcmp(macro.action, "stop_task") == 0) screen = Screen::ConfirmStop;
  else if (strcmp(macro.action, "send_followup") == 0) {
    followupText = "";
    screen = Screen::Followup;
  } else if (strcmp(macro.action, "approve") == 0 || strcmp(macro.action, "reject") == 0) {
    if (client.approval().open) screen = Screen::Approval;
  } else if (strcmp(macro.action, "retry") == 0) {
    client.sendFollowup(task.id, "Retry the failed work. Diagnose the cause and make the smallest safe fix.");
  } else if (strcmp(macro.action, "review_changes") == 0) {
    client.sendFollowup(task.id, "Review the current changes for correctness, regressions, security risks, and missing tests.");
  } else if (strcmp(macro.action, "run_tests") == 0) {
    client.sendFollowup(task.id, "Run the relevant tests, diagnose failures, and make the smallest safe fixes.");
  } else if (strcmp(macro.action, "commit_changes") == 0) {
    client.sendFollowup(task.id, "Review the finished changes and prepare an appropriate commit if the checks pass.");
  }
}

void handleDashboard(const InputEvent& event) {
  if (event.up) client.tasks().moveSelection(-1);
  if (event.down) client.tasks().moveSelection(1);
  if (event.tab) client.tasks().selectNextAttention();
  if (hasText(event, '1') || hasText(event, 'n')) {
    macroSelected = 0;
    macroPage = 0;
    screen = Screen::Macros;
  } else if (hasText(event, 'w')) {
    openWifiList();
  } else if (hasText(event, 'd')) {
    screen = Screen::Diagnostics;
  } else if (event.enterPressed && client.tasks().selected()) {
    client.sendSelect(client.tasks().selected()->id);
    taskMacroSelected = 0;
    screen = client.tasks().selected()->status == codexdeck::TaskStatus::WaitingApproval && client.approval().open
                 ? Screen::Approval
                 : Screen::Detail;
  }
}

void handleDetail(const InputEvent& event) {
  const codexdeck::TaskState* task = client.tasks().selected();
  if (!task || event.back) {
    screen = Screen::Dashboard;
    return;
  }
  if (task->macroCount) {
    if (event.up) taskMacroSelected = (taskMacroSelected + task->macroCount - 1) % task->macroCount;
    if (event.down) taskMacroSelected = (taskMacroSelected + 1) % task->macroCount;
  }
  bool canStop = false;
  bool canFollowup = false;
  for (size_t index = 0; index < task->macroCount; ++index) {
    canStop = canStop || strcmp(task->macros[index].action, "stop_task") == 0;
    canFollowup = canFollowup || strcmp(task->macros[index].action, "send_followup") == 0;
  }
  if (hasText(event, 's') && client.connected() && canStop) screen = Screen::ConfirmStop;
  else if (hasText(event, 'f') && client.connected() && canFollowup) {
    followupText = "";
    screen = Screen::Followup;
  } else if (task->status == codexdeck::TaskStatus::WaitingApproval && client.approval().open && event.enterPressed) {
    screen = Screen::Approval;
  } else if (event.enterPressed) {
    activateTaskMacro(*task);
  }
}

void handleMacros(const InputEvent& event) {
  const size_t count = client.globalMacroCount();
  if (event.back) {
    screen = Screen::Dashboard;
    return;
  }
  if (count == 0) return;
  if (event.up) macroSelected = (macroSelected + count - 1) % count;
  if (event.down) macroSelected = (macroSelected + 1) % count;
  if (event.left && macroPage > 0) macroSelected = (--macroPage) * 8;
  if (event.right && (macroPage + 1) * 8 < count) macroSelected = (++macroPage) * 8;
  for (char number = '1'; number <= '8'; ++number) {
    if (!hasText(event, number)) continue;
    const size_t selected = macroPage * 8 + static_cast<size_t>(number - '1');
    if (selected < count) {
      macroSelected = selected;
      launchSelectedMacro();
    }
    return;
  }
  macroPage = macroSelected / 8;
  if (event.enterPressed) launchSelectedMacro();
}

void handleApproval(const InputEvent& event, uint32_t nowMs) {
  if (!client.approval().open || event.back) {
    approvalConfirmation.reset();
    armedDecision = "";
    highRiskHolding = false;
    screen = Screen::Detail;
    return;
  }
  const bool highRisk = strcmp(client.approval().risk, "high") == 0;
  if (hasText(event, 'n')) {
    if (highRisk) submitApproval("decline");
    else confirmNormalDecision("decline", nowMs);
  } else if (hasText(event, 'c')) {
    if (highRisk) submitApproval("cancel");
    else confirmNormalDecision("cancel", nowMs);
  } else if (!highRisk && event.enterPressed) {
    confirmNormalDecision("accept", nowMs);
  }
  if (highRisk) {
    if (event.enterHeld) {
      approvalConfirmation.beginHold(nowMs);
      highRiskHolding = true;
      if (approvalConfirmation.updateHold(nowMs)) submitApproval("accept");
    } else if (highRiskHolding) {
      approvalConfirmation.cancelHold();
      highRiskHolding = false;
    }
  }
}

void handleFollowup(const InputEvent& event) {
  const codexdeck::TaskState* task = client.tasks().selected();
  if (!task || event.back) {
    screen = Screen::Detail;
    return;
  }
  if (event.erase) eraseLast(followupText);
  appendPrintable(followupText, event.text, 240);
  if (event.enterPressed && followupText.length() && client.connected()) {
    client.sendFollowup(task->id, followupText);
    screen = Screen::Detail;
  }
}

void handleWifiList(const InputEvent& event) {
  const size_t count = network.networkCount();
  if (event.back) {
    screen = client.connected() ? Screen::Dashboard : Screen::Offline;
    return;
  }
  if (hasText(event, 'r')) openWifiList();
  else if (hasText(event, 'm')) {
    selectedSsid = "";
    screen = Screen::WifiSsid;
  } else if (count) {
    if (event.up) wifiSelected = (wifiSelected + count - 1) % count;
    if (event.down) wifiSelected = (wifiSelected + 1) % count;
    if (event.enterPressed) {
      const WifiNetworkEntry* entry = network.networkAt(wifiSelected);
      if (!entry) return;
      selectedSsid = entry->ssid;
      wifiPassword = "";
      if (entry->secured) screen = Screen::WifiPassword;
      else {
        network.connectTo(selectedSsid, "");
        screen = Screen::Offline;
      }
    }
  }
}

void handleWifiSsid(const InputEvent& event) {
  if (event.back) {
    screen = Screen::WifiList;
    return;
  }
  if (event.erase) eraseLast(selectedSsid);
  appendPrintable(selectedSsid, event.text, 32);
  if (event.enterPressed && selectedSsid.length()) {
    wifiPassword = "";
    screen = Screen::WifiPassword;
  }
}

void handleWifiPassword(const InputEvent& event) {
  if (event.back) {
    screen = Screen::WifiList;
    return;
  }
  if (event.erase) eraseLast(wifiPassword);
  appendPrintable(wifiPassword, event.text, 63);
  if (event.enterPressed) {
    network.connectTo(selectedSsid, wifiPassword);
    wifiPassword = "";
    screen = Screen::Offline;
  }
}

void handleOtherScreens(const InputEvent& event) {
  if (screen == Screen::ConfirmStop) {
    const codexdeck::TaskState* task = client.tasks().selected();
    if (event.back || !task) screen = Screen::Detail;
    else if (event.enterPressed && client.connected()) {
      client.sendStop(task->id);
      screen = Screen::Detail;
    }
  } else if (screen == Screen::Offline) {
    if (hasText(event, 'r')) {
      network.retry();
      client.retryNow();
    } else if (hasText(event, 'w')) openWifiList();
    else if (hasText(event, 'd')) screen = Screen::Diagnostics;
  } else if (screen == Screen::Diagnostics) {
    if (event.back) screen = client.connected() ? Screen::Dashboard : Screen::Offline;
    else if (hasText(event, 'r')) {
      network.retry();
      client.retryNow();
    } else if (hasText(event, 'k')) screen = Screen::Keymap;
  } else if (screen == Screen::Keymap) {
    if (event.back) screen = Screen::Diagnostics;
    else if (hasInput(event)) {
      latestKey = "text='" + event.text + "'";
      latestKey += " U" + String(event.up) + " D" + String(event.down);
      latestKey += " L" + String(event.left) + " R" + String(event.right);
      latestKey += " E" + String(event.enterPressed) + " Del" + String(event.erase);
    }
  }
}

void render(uint32_t nowMs) {
  switch (screen) {
    case Screen::Dashboard: ui.dashboard(client.tasks(), client.connected(), client.toast()); break;
    case Screen::Detail: {
      const codexdeck::TaskState* task = client.tasks().selected();
      task ? ui.taskDetail(*task, taskMacroSelected) : ui.dashboard(client.tasks(), client.connected(), client.toast());
      break;
    }
    case Screen::Macros: ui.macros(client, macroSelected, macroPage); break;
    case Screen::Approval:
      ui.approval(client.approval(), armedDecision.c_str(), approvalConfirmation.armed(nowMs),
                  approvalConfirmation.holdPercent(nowMs));
      break;
    case Screen::Followup: {
      const codexdeck::TaskState* task = client.tasks().selected();
      task ? ui.followup(*task, followupText) : ui.dashboard(client.tasks(), client.connected(), client.toast());
      break;
    }
    case Screen::Offline: ui.offline(network, client); break;
    case Screen::WifiList: ui.wifiList(network, wifiSelected); break;
    case Screen::WifiSsid: ui.wifiSsid(selectedSsid); break;
    case Screen::WifiPassword: ui.wifiPassword(selectedSsid, wifiPassword); break;
    case Screen::ConfirmStop: {
      const codexdeck::TaskState* task = client.tasks().selected();
      task ? ui.confirmStop(*task) : ui.dashboard(client.tasks(), client.connected(), client.toast());
      break;
    }
    case Screen::Diagnostics: ui.diagnostics(network, client); break;
    case Screen::Keymap: ui.keymap(latestKey); break;
  }
  lastDrawMs = nowMs;
  seenRevision = client.revision();
}
}  // namespace

void controlDeckSetup() {
  Serial.begin(115200);
  delay(150);
  auto config = M5.config();
  M5Cardputer.begin(config, true);
  ui.begin();
  network.begin();
  if (!network.hasCredentials()) openWifiList();
  else if (network.bridgeHost().length()) {
    activeBridgeHost = network.bridgeHost();
    activeBridgePort = network.bridgePort();
    client.begin(activeBridgeHost, activeBridgePort);
  }
  Serial.println("[diag] action=boot status=ok app=control-deck proof=compile-ready");
  render(millis());
}

void controlDeckLoop() {
  M5Cardputer.update();
  const uint32_t nowMs = millis();
  network.update();
  connectKnownBridge();
  client.update(network.connected());

  if (client.connected() && screen == Screen::Offline) screen = Screen::Dashboard;
  if (!client.connected() && (screen == Screen::Dashboard || screen == Screen::Detail || screen == Screen::Macros)) {
    screen = Screen::Offline;
  }
  if (client.approval().open && screen == Screen::Dashboard) {
    client.tasks().selectById(client.approval().taskId);
    taskMacroSelected = 0;
    screen = Screen::Approval;
  }

  const InputEvent event = input.read();
  switch (screen) {
    case Screen::Dashboard: handleDashboard(event); break;
    case Screen::Detail: handleDetail(event); break;
    case Screen::Macros: handleMacros(event); break;
    case Screen::Approval: handleApproval(event, nowMs); break;
    case Screen::Followup: handleFollowup(event); break;
    case Screen::WifiList: handleWifiList(event); break;
    case Screen::WifiSsid: handleWifiSsid(event); break;
    case Screen::WifiPassword: handleWifiPassword(event); break;
    default: handleOtherScreens(event); break;
  }

  if (hasInput(event) || seenRevision != client.revision() || nowMs - lastDrawMs >= 1000 ||
      (screen == Screen::Approval && highRiskHolding)) {
    render(nowMs);
  }
  delay(20);
}
