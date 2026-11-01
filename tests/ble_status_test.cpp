#include "BleStatusPublisher.h"
#include "BleGatt.h"
#include <bluefruit.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

static uint16_t u16(const uint8_t* b) { return uint16_t(b[0]) | (uint16_t(b[1]) << 8); }
static uint32_t u32(const uint8_t* b) {
  return uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
}
static void initialize(BleStatusPublisher& p) {
  p.cacheDiagnostics(0x11223344, 0x55667788);
  assert(p.cacheBattery(3.7f));
  p.connect(); p.setEnabled(true);
}
static std::vector<std::vector<uint8_t>> drain(BleStatusPublisher& p, uint32_t now, uint16_t mtu) {
  std::vector<std::vector<uint8_t>> packets;
  uint8_t bytes[30]; uint16_t len;
  while (p.next(now, mtu, bytes, len)) {
    assert(packets.size() < 2);
    packets.emplace_back(bytes, bytes + len); p.accepted();
  }
  return packets;
}
static void exactVectorAndInitialization() {
  BleStatusPublisher zero;
  zero.cacheDiagnostics(0, 0); assert(zero.cacheBattery(0)); zero.connect(); zero.setEnabled(true);
  uint8_t zeroPacket[30]; uint16_t zeroLength;
  assert(zero.next(0, 247, zeroPacket, zeroLength));
  assert(u32(zeroPacket + 20) == 0 && u32(zeroPacket + 24) == 0 && u16(zeroPacket + 28) == 0);
  BleStatusPublisher p;
  uint8_t b[30]; uint16_t len;
  p.connect(); p.setEnabled(true);
  assert(!p.next(100, 247, b, len));
  p.cacheDiagnostics(0x11223344, 0x55667788);
  assert(!p.next(100, 247, b, len)); // No default 3.7V placeholder.
  assert(!p.cacheBattery(std::numeric_limits<float>::quiet_NaN()));
  assert(!p.cacheBattery(std::numeric_limits<float>::infinity()));
  assert(!p.cacheBattery(-1)); assert(!p.cacheBattery(65.535f)); assert(!p.ready());
  assert(p.cacheBattery(3.7f)); p.setMode(0);
  assert(p.next(0x01020304, 247, b, len) && len == 30);
  const uint8_t expected[] = {
    0,0,0,1,0x18,0, 2,1,2,0,0,0,4,3,2,1,10,0,1,0,
    0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0x74,0x0e
  };
  assert(sizeof(expected) == len && std::memcmp(expected, b, len) == 0);
  p.accepted();
  assert(!p.next(0x01020304 + 999, 247, b, len));
  assert(p.next(0x01020304 + 1000, 247, b, len));
  assert(b[8] == 0 && u16(b) == 1 && u16(b + 10) == 1);
  assert(p.cacheBattery(65.534f)); assert(!p.cacheBattery(65.535f));
  p.accepted();
  assert(p.next(0x01020304 + 2000, 247, b, len)); assert(u16(b + 28) == 65534);
}
static void mtuMatrix() {
  const uint16_t mtus[] = {23,24,32,33,69,185,247};
  const std::vector<std::vector<size_t>> lengths = {{20,16},{21,15},{29,7},{30},{30},{30},{30}};
  for (size_t i = 0; i < 7; ++i) {
    BleStatusPublisher p; initialize(p);
    auto packets = drain(p, 100, mtus[i]);
    assert(packets.size() == lengths[i].size());
    std::vector<uint8_t> logical;
    for (size_t j = 0; j < packets.size(); ++j) {
      auto& b = packets[j]; assert(b.size() == lengths[i][j]);
      assert(u16(b.data()) == 0 && b[2] == j && b[3] == packets.size() && u16(b.data() + 4) == 24);
      logical.insert(logical.end(), b.begin() + 6, b.end());
    }
    assert(logical.size() == 24 && logical[0] == 2 && logical[1] == 1 && logical[3] == 5);
    assert(u16(logical.data() + 10) == 10 && u32(logical.data() + 14) == 0x11223344);
    assert(u32(logical.data() + 18) == 0x55667788 && u16(logical.data() + 22) == 3700);
    assert(drain(p, 1099, mtus[i]).empty());
    assert(!drain(p, 1100, mtus[i]).empty());
  }
}
static void retriesAndMtuChanges() {
  BleStatusPublisher p; initialize(p);
  uint8_t b[30], saved[30]; uint16_t len;
  assert(p.next(100, 23, b, len) && len == 20); std::memcpy(saved, b, len);
  p.cacheDiagnostics(9, 11); assert(p.cacheBattery(4.2f));
  assert(p.next(600, 23, b, len) && std::memcmp(saved, b, len) == 0); // Resources retry, no mutation.
  p.accepted();
  assert(p.next(750, 247, b, len) && len == 16 && b[2] == 1 && b[3] == 2);
  assert(u32(b + 6) == 0x11223344 && u16(b + 14) == 3700); // Frozen old payload/slice size.
  p.accepted(); assert(!p.next(1099, 247, b, len));
  assert(p.next(1100, 247, b, len) && len == 30 && b[3] == 1);
  assert(u32(b + 20) == 9 && u32(b + 24) == 11 && u16(b + 28) == 4200);

  BleStatusPublisher shrink; initialize(shrink);
  assert(shrink.next(0, 32, b, len)); shrink.accepted();
  assert(!shrink.next(1, 23, b, len) && shrink.needsReconnect());
  shrink.disconnect(); shrink.connect(); shrink.setEnabled(true);
  assert(shrink.next(2, 23, b, len) && !shrink.needsReconnect() && u16(b) == 0);

  BleStatusPublisher timed; initialize(timed);
  assert(timed.next(0, 23, b, len)); timed.accepted();
  timed.cacheDiagnostics(42, 43);
  assert(timed.next(1999, 23, b, len) && b[2] == 1);
  assert(timed.next(2000, 247, b, len) && b[2] == 0 && u16(b) == 1 && u32(b + 20) == 42);
}
static void modesSubscriptionsAndReconnect() {
  BleStatusPublisher p; initialize(p); p.setMode(0);
  uint8_t b[30]; uint16_t len;
  assert(p.next(100, 23, b, len) && u16(b + 18) == 1); p.accepted();
  p.setMode(2);
  assert(p.next(101, 23, b, len) && b[2] == 0 && b[9] == 2 && b[8] == 2);
  assert(u16(b + 10) == 1 && u16(b + 18) == 2);
  p.setMode(2); // Same mode does not cancel a frame or increment epoch.
  assert(p.next(102, 23, b, len) && u16(b + 10) == 1 && u16(b + 18) == 2);
  p.setEnabled(false); assert(!p.next(103, 247, b, len));
  p.setEnabled(true);
  assert(p.next(104, 247, b, len) && u16(b + 10) == 2 && u16(b) == 2 && b[8] == 2);
  p.accepted(); p.setMode(0); p.setMode(7); // Invalid value ignored; return to Cable gets newer epoch.
  assert(p.next(105, 247, b, len) && b[9] == 0 && u16(b + 18) == 3);
  p.disconnect(); assert(!p.next(106, 247, b, len));
  p.connect(); p.setEnabled(true); // Includes the restored-CCCD eligibility path.
  assert(p.next(107, 247, b, len) && u16(b) == 0 && u16(b + 10) == 0 && u16(b + 18) == 3);
  p.accepted(); p.setMode(6);
  assert(p.next(108, 247, b, len) && b[9] == 6 && u16(b + 18) == 4);
}
static void wraps() {
  BleStatusPublisher p; initialize(p);
  uint8_t b[30]; uint16_t len;
  const uint32_t start = 0xfffffff0;
  assert(p.next(start, 247, b, len)); p.accepted();
  assert(!p.next(start + uint32_t(999), 247, b, len));
  assert(p.next(start + uint32_t(1000), 247, b, len));
  BleStatusPublisher counters; initialize(counters);
  for (uint32_t i = 0; i <= 65536; ++i) {
    assert(counters.next(i * 1000, 247, b, len));
    assert(u16(b) == uint16_t(i) && u16(b + 10) == uint16_t(i)); counters.accepted();
  }
  for (uint32_t i = 0; i < 65536; ++i) counters.setMode(uint8_t(i & 1));
  assert(counters.next(65536001, 247, b, len) && u16(b + 18) == 0);
}
static void sdkTransport() {
  assert(BleGatt::begin(1,2,2,"test"));
  uint8_t packet[30] = {};
  FakeBle::cccd = 0;
  assert(BleGatt::tryNotifyStatus(1, packet, 30) == BleGatt::SendResult::Stopped && FakeBle::attempts == 0);
  FakeBle::cccd = 1;
  for (auto code : {NRF_ERROR_RESOURCES, NRF_ERROR_BUSY, NRF_ERROR_INVALID_STATE}) {
    FakeBle::sendResult = code;
    assert(BleGatt::tryNotifyStatus(1, packet, 30) == BleGatt::SendResult::Retry);
  }
  for (auto code : {NRF_ERROR_FORBIDDEN, BLE_ERROR_INVALID_CONN_HANDLE, BLE_ERROR_GATTS_SYS_ATTR_MISSING}) {
    FakeBle::sendResult = code;
    assert(BleGatt::tryNotifyStatus(1, packet, 30) == BleGatt::SendResult::Stopped);
  }
  FakeBle::sendResult = NRF_ERROR_DATA_SIZE;
  assert(BleGatt::tryNotifyStatus(1, packet, 30) == BleGatt::SendResult::Restart);
  FakeBle::sendResult = NRF_SUCCESS;
  assert(BleGatt::tryNotifyStatus(1, packet, 30) == BleGatt::SendResult::Sent);
  assert(FakeBle::lastPacket.size() == 30 && FakeBle::offeredHandle == BLECharacteristic::all()[7]->handles().value_handle);
  FakeBle::truncate = true;
  assert(BleGatt::tryNotifyStatus(1, packet, 30) == BleGatt::SendResult::Restart);
  FakeBle::readResult = NRF_ERROR_INVALID_STATE;
  assert(!BleGatt::statusSubscribed(1));
}
int main() {
  exactVectorAndInitialization(); mtuMatrix(); retriesAndMtuChanges(); modesSubscriptionsAndReconnect(); wraps(); sdkTransport();
  std::cout << "BLE Status: exact bytes, MTU matrix, retries, initialization, epochs, subscriptions and wraps passed\n";
}
