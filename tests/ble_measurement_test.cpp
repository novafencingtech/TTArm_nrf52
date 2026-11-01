#include "BleMeasurementPublisher.h"
#include "BleGatt.h"
#include <bluefruit.h>
#include <cassert>
#include <cstring>
#include <limits>
#include <vector>
#include <iostream>
static uint16_t u16(const uint8_t* p) { return p[0] | uint16_t(p[1]) << 8; }
static uint32_t u32(const uint8_t* p) { return p[0] | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
static std::vector<uint8_t> frame(BleMeasurementPublisher& p, uint32_t now, uint32_t us, uint16_t mtu = 247) {
  std::vector<uint8_t> out;
  uint8_t b[66]; uint16_t length;
  if (!p.next(now, us, mtu, b, length)) return out;
  const uint16_t total = u16(b+4), transport = u16(b);
  const uint8_t count = b[3];
  for (uint8_t i = 0; i < count; ++i) {
    if (i) assert(p.next(now, us, mtu, b, length));
    assert(b[2] == i && b[3] == count && u16(b) == transport && u16(b+4) == total);
    out.insert(out.end(), b+6, b+length); p.accepted();
  }
  assert(out.size() == total); return out;
}
static void weaponAndProbe() {
  for (uint16_t mtu : {23, 24, 32, 69, 247}) {
    BleMeasurementPublisher w(0x12); w.setMode(2, 7); w.connect(); w.setEnabled(true);
    assert(frame(w, 0, 0, mtu).empty());
    w.cacheWeapon(31, -0.125f, 999, 123, 456, 999);
    auto b = frame(w, 10, 0, mtu);
    assert(b.size() == 33 && b[1] == 0x12 && b[2] == 2 && b[3] == 2);
    assert(u16(b.data()+10) == 19 && u16(b.data()+12) == 7 && u16(b.data()+14) == 0);
    assert(b[16] == 31 && int32_t(u32(b.data()+17)) == -125 && u32(b.data()+21) == 0x7fffffff);
    assert(u32(b.data()+25) == 123 && u32(b.data()+29) == 456);
    assert(frame(w, 43, 33332, mtu).empty());
    b = frame(w, 44, 33333, mtu); assert(b[2] == 0 && u16(b.data()+14) == 1);
    b = frame(w, 77, 66666, mtu); assert(u16(b.data()+14) == 2);
    assert(frame(w, 110, 99999, mtu).empty());
    b = frame(w, 110, 100000, mtu); assert(u16(b.data()+14) == 3);
    w.setMode(3, 8); assert(frame(w, 111, 111000, mtu).empty());
    w.cacheWeapon(0, 1, 2, 0, 0, 999); b = frame(w, 111, 111000, mtu);
    assert(b[3] == 3 && u16(b.data()+14) == 3);
    w.disconnect(); w.connect(); w.setEnabled(true); b = frame(w, 112, 112000, mtu);
    assert(u16(b.data()+4) == 0 && u16(b.data()+14) == 3);
    w.cacheWeapon(0, std::numeric_limits<float>::quiet_NaN(), 1, 0, 0, 999);
    assert(frame(w, 200, 200000, mtu).empty());

    BleMeasurementPublisher p(0x13); p.setMode(4, 9); p.connect(); p.setEnabled(true);
    const float values[] = {1, 2, 999, -0.125f, 4, 5}; p.cacheProbe(values, 999);
    b = frame(p, 0, 0, mtu); assert(b.size() == 40 && b[1] == 0x13 && b[3] == 4);
    const uint32_t expected[] = {1000, 2000, 0x7fffffff, uint32_t(-125), 4000, 5000};
    for (int i = 0; i < 6; ++i) assert(u32(b.data()+16+4*i) == expected[i]);
  }
}
static void deadlineWrapAndValidation() {
  BleMeasurementPublisher p(0x12); p.setMode(2,1); p.connect(); p.setEnabled(true);
  p.cacheWeapon(0,1,2,0,0,999);
  frame(p,0,0xfffffff0);
  assert(frame(p,33,33316).empty());
  auto b = frame(p,34,33317); assert(u16(b.data()+14) == 1);
  b = frame(p,1034,1033317); assert(u16(b.data()+14) == 2); // Skip missed slots.
  assert(frame(p,1034,1033317).empty());
  p.cacheWeapon(32,1,2,0,0,999); assert(frame(p,1068,1067000).empty());
  p.cacheWeapon(0,-3000000,2,0,0,999); assert(frame(p,1100,1100000).empty());
  p.cacheWeapon(0,1,2,0,0,999); p.setEnabled(false); p.setEnabled(true);
  b=frame(p,1101,1101000); assert(b[2] == 2 && u16(b.data()+14) == 2);
}
static void backpressure() {
  BleMeasurementPublisher p(0x12); p.setMode(2, 1); p.connect(); p.setEnabled(true);
  p.cacheWeapon(0, 1, 2, 0, 0, 999);
  uint8_t b[66], saved[66]; uint16_t len;
  assert(p.next(0, 0, 23, b, len)); memcpy(saved,b,len);
  p.cacheWeapon(3, 9, 9, 99, 99, 999);
  assert(p.next(50, 50000, 247, b, len) && !memcmp(b,saved,len));
  p.accepted(); assert(p.next(51, 51000, 247, b, len) && b[2] == 1 && len == 20);
  assert(!p.next(52, 52000, 22, b, len) && p.needsReconnect());
  p.disconnect(); p.connect(); p.setEnabled(true);
  assert(p.next(53, 53000, 23, b, len));
  assert(p.next(153, 153000, 23, b, len) && b[2] == 0 && u16(b) == 1);
  p.setEnabled(false); assert(!p.next(154,154000,23,b,len));
  p.setEnabled(true); auto out = frame(p, 155,155000,23); assert(out[2] == 2 && out[16] == 3);
}
static void lights() {
  BleMeasurementPublisher p(0x14); p.setMode(1, 3); p.connect(); p.setEnabled(true);
  p.cacheLights(0, 0, 0, 0); auto b = frame(p,0,0,23);
  assert(b.size() == 26 && b[1] == 0x14 && b[2] == 2 && b[3] == 1 && u16(b.data()+14) == 0);
  p.cacheLights(5, 10, 0, 10); p.cacheLights(4, 11, 0, 11); p.cacheLights(0, 11, 0, 12);
  for (int i = 1; i <= 3; ++i) {
    b = frame(p,12,12000,23); assert(b[2] == 2 && u16(b.data()+14) == i);
    assert(b[16] == (i == 1 ? 5 : i == 2 ? 4 : 0) && b[17] == 0);
    assert(u32(b.data()+6) == uint32_t(9+i));
  }
  assert(frame(p,1011,1011000).empty()); b = frame(p,1012,1012000);
  assert(b[2] == 0 && u16(b.data()+14) == 3 && u32(b.data()+6) == 1012);
  // Overflow discards old queued states; the newest complete state survives.
  for (int i = 0; i < 17; ++i) p.cacheLights(i%2, 1100+i, 0, 1100+i);
  b = frame(p,1117,1117000); assert(u16(b.data()+14) == 20 && u32(b.data()+18) == 1116);
  p.cacheLights(1, 1200, 0, 1200); p.cacheLights(0, 1201, 0, 1201);
  b = frame(p,1300,1300000); assert(u16(b.data()+14) == 22 && b[16] == 0 && b[2] == 0);
  p.setMode(5,4); assert(frame(p,1400,1400000).empty());
  p.setMode(1,5); assert(frame(p,1401,1401000).empty());
  p.cacheLights(0,1201,0,1401); b=frame(p,1401,1401000); assert(u16(b.data()+14)==22);
  // Millisecond wrap and microsecond wrap use modular elapsed arithmetic.
  BleMeasurementPublisher wrap(0x14); wrap.setMode(1,1); wrap.connect(); wrap.setEnabled(true);
  wrap.cacheLights(0,0,0,0xfffffff0); frame(wrap,0xfffffff0,0);
  assert(frame(wrap,983,0).empty()); assert(!frame(wrap,984,0).empty());
}
static void gatt() {
  assert(BleGatt::begin(1,2,2,"streams"));
  uint8_t packet[32] = {};
  const BleGatt::Stream streams[] = {BleGatt::Stream::Weapon, BleGatt::Stream::Probe, BleGatt::Stream::Lights};
  const int indices[] = {3,5,6};
  for (int i = 0; i < 3; ++i) {
    const auto handles = BLECharacteristic::all()[indices[i]]->handles();
    assert(BleGatt::cccdHandle(streams[i]) == handles.cccd_handle);
    FakeBle::cccd = 0; FakeBle::sendResult = 0;
    assert(!BleGatt::subscribed(1,streams[i]));
    assert(BleGatt::tryNotify(1,streams[i],packet,32) == BleGatt::SendResult::Stopped);
    FakeBle::cccd = 1;
    assert(BleGatt::subscribed(1,streams[i]));
    assert(BleGatt::tryNotify(1,streams[i],packet,32) == BleGatt::SendResult::Sent);
    assert(FakeBle::offeredHandle == handles.value_handle);
    FakeBle::sendResult = NRF_ERROR_RESOURCES;
    assert(BleGatt::tryNotify(1,streams[i],packet,32) == BleGatt::SendResult::Retry);
  }
}
int main() { static_assert(sizeof(BleMeasurementPublisher) <= 512, "Bounded stream memory"); weaponAndProbe(); deadlineWrapAndValidation(); backpressure(); lights(); gatt(); std::cout << "Measurement tests passed\n"; }
