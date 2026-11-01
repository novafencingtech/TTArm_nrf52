#pragma once
#include <stdint.h>
#include <string>
#include <vector>
enum SecureMode_t { SECMODE_NO_ACCESS = 0, SECMODE_OPEN = 0x11, SECMODE_ENC_NO_MITM = 0x21 };
enum { ERROR_NONE = 0, CHR_PROPS_READ = 2, CHR_PROPS_WRITE = 8, CHR_PROPS_NOTIFY = 16, CHR_PROPS_INDICATE = 32 };
enum { NRF_SUCCESS = 0, NRF_ERROR_RESOURCES = 19, NRF_ERROR_BUSY = 17, NRF_ERROR_INVALID_STATE = 8,
       NRF_ERROR_FORBIDDEN = 15, NRF_ERROR_DATA_SIZE = 12, BLE_ERROR_INVALID_CONN_HANDLE = 0x3002,
       BLE_ERROR_GATTS_SYS_ATTR_MISSING = 0x3401, BLE_GATT_HVX_NOTIFICATION = 1 };
struct ble_gatts_char_handles_t { uint16_t value_handle, cccd_handle; };
struct ble_gatts_value_t { uint16_t len = 0, offset = 0; uint8_t* p_value = nullptr; };
struct ble_gatts_hvx_params_t {
  uint16_t handle = 0;
  uint8_t type = 0;
  uint16_t offset = 0;
  uint16_t* p_len = nullptr;
  const uint8_t* p_data = nullptr;
};
namespace FakeBle {
inline uint16_t cccd = 0, offeredHandle = 0;
inline uint32_t readResult = 0, sendResult = 0, attempts = 0;
inline bool truncate = false;
inline std::vector<uint8_t> lastPacket;
}
inline uint32_t sd_ble_gatts_value_get(uint16_t, uint16_t, ble_gatts_value_t* value) {
  if (FakeBle::readResult) return FakeBle::readResult;
  value->p_value[0] = uint8_t(FakeBle::cccd);
  value->p_value[1] = uint8_t(FakeBle::cccd >> 8);
  value->len = 2;
  return NRF_SUCCESS;
}
inline uint32_t sd_ble_gatts_hvx(uint16_t, ble_gatts_hvx_params_t* params) {
  ++FakeBle::attempts;
  FakeBle::offeredHandle = params->handle;
  if (FakeBle::sendResult) return FakeBle::sendResult;
  if (FakeBle::truncate) --*params->p_len;
  FakeBle::lastPacket.assign(params->p_data, params->p_data + *params->p_len);
  return NRF_SUCCESS;
}
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
  ble_gatts_char_handles_t handles() {
    for (size_t i = 0; i < all().size(); ++i)
      if (all()[i] == this) return {uint16_t(32 + 4 * i), uint16_t(33 + 4 * i)};
    return {0,0};
  }
  // No blocking Bluefruit notify() method: Phase 2 uses nonblocking SDK HVX.
};
