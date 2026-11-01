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
  // Control and the other measurement streams remain deferred.
  return add(control, CHR_PROPS_WRITE, 9, SECMODE_NO_ACCESS, SECMODE_NO_ACCESS) &&
         add(result, CHR_PROPS_INDICATE, 10, protectedAccess, SECMODE_NO_ACCESS);
}
namespace {
bool subscribed(uint16_t connection, uint16_t cccd) {
  uint8_t bytes[2] = {};
  ble_gatts_value_t value = {};
  value.len = sizeof(bytes);
  value.p_value = bytes;
  return sd_ble_gatts_value_get(connection, cccd, &value) == NRF_SUCCESS &&
         value.len == 2 && (bytes[0] & 0x01) != 0;
}
BleGatt::SendResult notify(uint16_t connection, BLECharacteristic& chr, const uint8_t* packet, uint16_t length) {
  using SendResult = BleGatt::SendResult;
  if (!subscribed(connection, chr.handles().cccd_handle)) return SendResult::Stopped;
  uint16_t acceptedLength = length;
  ble_gatts_hvx_params_t params = {};
  params.handle = chr.handles().value_handle;
  params.type = BLE_GATT_HVX_NOTIFICATION;
  params.p_len = &acceptedLength;
  params.p_data = packet;
  // Bluefruit notify() can wait for a queue credit. This stock SDK call returns
  // NRF_ERROR_RESOURCES immediately; the foreground publisher retries later.
  const uint32_t result = sd_ble_gatts_hvx(connection, &params);
  if (result == NRF_SUCCESS) return acceptedLength == length ? SendResult::Sent : SendResult::Restart;
  // INVALID_STATE can mean an MTU exchange is in progress. Keep the frame's
  // slice boundaries and retry; actual CCCD/disconnect changes are observed too.
  if (result == NRF_ERROR_RESOURCES || result == NRF_ERROR_BUSY || result == NRF_ERROR_INVALID_STATE) return SendResult::Retry;
  if (result == NRF_ERROR_FORBIDDEN || result == BLE_ERROR_INVALID_CONN_HANDLE ||
      result == BLE_ERROR_GATTS_SYS_ATTR_MISSING) return SendResult::Stopped;
  return SendResult::Restart;
}
}
uint16_t BleGatt::statusCccdHandle() { return status.handles().cccd_handle; }
uint16_t BleGatt::cableCccdHandle() { return cable.handles().cccd_handle; }
bool BleGatt::statusSubscribed(uint16_t connection) { return subscribed(connection, statusCccdHandle()); }
bool BleGatt::cableSubscribed(uint16_t connection) { return subscribed(connection, cableCccdHandle()); }
BleGatt::SendResult BleGatt::tryNotifyStatus(uint16_t connection, const uint8_t* packet, uint16_t length) {
  return notify(connection, status, packet, length);
}
BleGatt::SendResult BleGatt::tryNotifyCable(uint16_t connection, const uint8_t* packet, uint16_t length) {
  return notify(connection, cable, packet, length);
}
#else
uint16_t BleGatt::cableCccdHandle() { return 0; }
bool BleGatt::cableSubscribed(uint16_t) { return false; }
BleGatt::SendResult BleGatt::tryNotifyCable(uint16_t, const uint8_t*, uint16_t) { return SendResult::Stopped; }
bool BleGatt::begin(uint64_t, uint64_t, uint16_t, const char*) { return false; }
uint16_t BleGatt::statusCccdHandle() { return 0; }
bool BleGatt::statusSubscribed(uint16_t) { return false; }
BleGatt::SendResult BleGatt::tryNotifyStatus(uint16_t, const uint8_t*, uint16_t) { return SendResult::Stopped; }
#endif
