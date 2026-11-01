#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
inline void delay(uint32_t ms) { testMillis += ms; }
struct TestSerial {
  template<typename T> void print(const T&) {}
  template<typename T> void println(const T&) {}
  void println() {}
};
inline TestSerial Serial;
