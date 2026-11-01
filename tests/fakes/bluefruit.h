#pragma once
#include <stdint.h>
#include <string>
#include <vector>
enum SecureMode_t { SECMODE_NO_ACCESS = 0, SECMODE_OPEN = 0x11, SECMODE_ENC_NO_MITM = 0x21 };
enum { ERROR_NONE = 0, CHR_PROPS_READ = 2, CHR_PROPS_WRITE = 8, CHR_PROPS_NOTIFY = 16, CHR_PROPS_INDICATE = 32 };
struct BLEService {
  explicit BLEService(const char*) {}
  int begin() { return 0; }
};
struct BLECharacteristic {
  static std::vector<BLECharacteristic*>& all() { static std::vector<BLECharacteristic*> list; return list; }
  std::string uuid;
  uint8_t properties = 0;
  uint16_t maxLength = 0;
  SecureMode_t readPermission = SECMODE_NO_ACCESS, writePermission = SECMODE_NO_ACCESS;
  std::vector<uint8_t> value;
  explicit BLECharacteristic(const char* id) : uuid(id) { all().push_back(this); }
  void setProperties(uint8_t p) { properties = p; }
  void setMaxLen(uint16_t len) { maxLength = len; }
  void setPermission(SecureMode_t r, SecureMode_t w) { readPermission = r; writePermission = w; }
  int begin() { return 0; }
  uint16_t write(const uint8_t* data, uint16_t len) { value.assign(data, data + len); return len; }
  // Deliberately no notify/indicate functions: Phase 1 has no publisher.
};
