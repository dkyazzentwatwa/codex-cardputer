#include "app.h"

#include <M5Cardputer.h>

namespace {
constexpr uint16_t COLOR_BG = 0x0000;
constexpr uint16_t COLOR_HEADER = 0x001F;
constexpr uint16_t COLOR_TEXT = 0xFFFF;
constexpr uint16_t COLOR_DIM = 0x8410;
constexpr uint16_t COLOR_ACCENT = 0x07FF;

void drawBoot() {
  auto& display = M5Cardputer.Display;
  display.fillScreen(COLOR_BG);
  display.fillRect(0, 0, display.width(), 20, COLOR_HEADER);
  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_HEADER);
  display.setCursor(6, 6);
  display.print("CODEX CONTROL DECK");
  display.setTextColor(COLOR_ACCENT, COLOR_BG);
  display.setCursor(8, 42);
  display.print("Cardputer ADV");
  display.setTextColor(COLOR_TEXT, COLOR_BG);
  display.setCursor(8, 62);
  display.print("Repository scaffold ready");
  display.setTextColor(COLOR_DIM, COLOR_BG);
  display.setCursor(8, 82);
  display.print("Bridge: not configured");
  display.fillRect(0, display.height() - 14, display.width(), 14, COLOR_HEADER);
  display.setTextColor(COLOR_DIM, COLOR_HEADER);
  display.setCursor(6, display.height() - 11);
  display.print("Arduino CLI firmware");
}
}  // namespace

void controlDeckSetup() {
  Serial.begin(115200);
  delay(150);
  auto config = M5.config();
  M5Cardputer.begin(config, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextDatum(top_left);
  M5Cardputer.Display.setTextWrap(false);
  M5Cardputer.Display.setBrightness(180);
  drawBoot();
  Serial.println("[diag] action=boot status=ok app=control-deck");
}

void controlDeckLoop() {
  M5Cardputer.update();
  delay(20);
}
