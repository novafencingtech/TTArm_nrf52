#pragma once
#include "Arduino.h"
#include <cassert>
#include <string>
#include <vector>

// Records actual production menu/UI drawing calls and checks panel bounds.
class Adafruit_SSD1351 {
public:
  struct Text { int x, y, scale; std::string value; };
  std::vector<Text> text;
  int x = 0, y = 0, scale = 1;
  void fillScreen(uint16_t) { text.clear(); }
  void setFont(const void*) {}
  void setTextWrap(bool) {}
  void setTextSize(int value) { scale = value; }
  void setTextColor(uint16_t, uint16_t = 0) {}
  void setCursor(int px, int py) { x = px; y = py; }
  void print(const char* value) {
    assert(x >= 0 && y >= 0 && y + 8 * scale <= 128);
    assert(x + int(strlen(value)) * 6 * scale <= 128);
    text.push_back({x, y, scale, value}); x += int(strlen(value)) * 6 * scale;
  }
  void print(char value) { char s[] = {value, 0}; print(s); }
  void print(uint8_t value) { print(std::to_string(value).c_str()); }
  template<typename T> void println(const T& value) { print(value); x = 0; y += 8 * scale; }
  void drawRect(int px, int py, int width, int height, uint16_t) {
    assert(px >= 0 && py >= 0 && px + width <= 128 && py + height <= 128);
  }
  void fillRect(int px, int py, int width, int height, uint16_t color) { drawRect(px, py, width, height, color); }
  void fillCircle(int px, int py, int radius, uint16_t) {
    assert(px - radius >= 0 && py - radius >= 0 && px + radius < 128 && py + radius < 128);
  }
};
