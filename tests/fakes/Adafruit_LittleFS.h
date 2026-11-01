#pragma once
#include "Arduino.h"
class TestFileSystem;
namespace Adafruit_LittleFS_Namespace {
enum { FILE_O_READ, FILE_O_WRITE };
class File {
public:
  explicit File(TestFileSystem&) {}
  File() {}
  operator bool() const { return false; }
  int read(void*, size_t) { return 0; }
  size_t write(const uint8_t*, size_t size) { return size; }
  void close() {}
  size_t size() { return 0; }
};
}
class TestFileSystem {
public:
  Adafruit_LittleFS_Namespace::File open(const char*, uint8_t) { return {}; }
  bool begin() { return true; }
  bool exists(const char*) { return false; }
  bool remove(const char*) { return true; }
};
