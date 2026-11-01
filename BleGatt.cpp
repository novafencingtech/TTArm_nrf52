#include "BleConfig.h"
#include "BleGatt.h"
#include "BleIdentity.h"
const char BleGatt::ServiceUuid[] = "4a1e0000-3bb5-44b4-b566-8d22e72da6f5";

#if TTARM_BLE_ENABLED
#include <bluefruit.h>
namespace {
BLEService tester(BleGatt::ServiceUuid);
BLECharacteristic protocol("4a1e0001-3bb5-44b4-b566-8d22e72da6f5");
BLECharacteristic identity("4a1e0002-3bb5-44b4-b566-8d22e72da6f5");
BLECharacteristic cable("4a1e0010-3bb5-44b4-b566-8d22e72da6f5");
BLECharacteristic weapon("4a1e0011-3bb5-44b4-b566-8d22e72da6f5");
BLECharacteristic capture("4a1e0012-3bb5-44b4-b566-8d22e72da6f5");
BLECharacteristic probe("4a1e0013-3bb5-44b4-b566-8d22e72da6f5");
BLECharacteristic lights("4a1e0014-3bb5-44b4-b566-8d22e72da6f5");
BLECharacteristic status("4a1e0020-3bb5-44b4-b566-8d22e72da6f5");
BLECharacteristic control("4a1e0030-3bb5-44b4-b566-8d22e72da6f5");
BLECharacteristic result("4a1e0031-3bb5-44b4-b566-8d22e72da6f5");
bool add(BLECharacteristic& chr, uint8_t properties, uint16_t length,
         SecureMode_t readPermission, SecureMode_t writePermission) {
  chr.setProperties(properties);
  chr.setMaxLen(length);
  // Bluefruit derives CCCD write permissions from the value's read permission.
  // Notify-only values still have no Read property.
  chr.setPermission(readPermission, writePermission);
  return chr.begin() == ERROR_NONE;
}
}
bool BleGatt::begin(uint64_t deviceId, uint64_t bootId, uint16_t revision, const char* build) {
  const SecureMode_t protectedAccess = SECMODE_ENC_NO_MITM;
  uint8_t bytes[68];
  BleIdentity::encode(bytes, deviceId, bootId, revision, build);
  if (tester.begin() != ERROR_NONE ||
      !add(protocol, CHR_PROPS_READ, 8, SECMODE_OPEN, SECMODE_NO_ACCESS) ||
      protocol.write(BleIdentity::PublicProtocol, 8) != 8 ||
      !add(identity, CHR_PROPS_READ, 68, protectedAccess, SECMODE_NO_ACCESS) ||
      identity.write(bytes, sizeof(bytes)) != sizeof(bytes)) return false;
  BLECharacteristic* streams[] = {&cable, &weapon, &capture, &probe, &lights, &status};
  for (auto chr : streams) {
    if (!add(*chr, CHR_PROPS_NOTIFY, 66, protectedAccess, SECMODE_NO_ACCESS)) return false;
  }
  // Final database, but ALL Control writes are rejected at ATT in Phase 1.
  // There is no callback that executes a command and no publisher/send path.
  return add(control, CHR_PROPS_WRITE, 9, SECMODE_NO_ACCESS, SECMODE_NO_ACCESS) &&
         add(result, CHR_PROPS_INDICATE, 10, protectedAccess, SECMODE_NO_ACCESS);
}
#else
bool BleGatt::begin(uint64_t, uint64_t, uint16_t, const char*) { return false; }
#endif
