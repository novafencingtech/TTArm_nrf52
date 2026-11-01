#include "BleCablePublisher.h"
#include <string.h>
#include <math.h>
namespace {
void put16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }
void put32(uint8_t* p, uint32_t v) {
  for (uint8_t i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
}
}
void BleCablePublisher::cache(uint16_t faults, const float resistance[6], const int adc[9], float open) {
  if (mode != 0 || (faults & ~0x1ff)) return;
  uint8_t bytes[44] = {};
  put16(bytes, faults);
  for (uint8_t i = 0; i < 6; ++i) {
    if (!isfinite(resistance[i])) { ready = pending = false; return; }
    int32_t value = INT32_MAX;
    if (resistance[i] < open) {
      const double scaled = double(resistance[i]) * 1000.0;
      const double rounded = scaled < 0 ? ceil(scaled - 0.5) : floor(scaled + 0.5);
      if (rounded < -2147483647.0 || rounded > 2147483646.0) { ready = pending = false; return; }
      value = int32_t(rounded);
    }
    put32(bytes + 2 + 4 * i, uint32_t(value));
  }
  for (uint8_t i = 0; i < 9; ++i) {
    if (adc[i] < 0 || adc[i] > 4095) { ready = pending = false; return; }
    put16(bytes + 26 + 2 * i, adc[i]);
  }
  memcpy(cached, bytes, sizeof(cached));
  ready = true;
}
void BleCablePublisher::setMode(uint8_t value, uint16_t generation) {
  if (mode == value && epoch == generation) return;
  mode = value; epoch = generation;
  ready = pending = clockStarted = dirty = false;
  immediate = true;
}
void BleCablePublisher::connect() {
  connected = true; enabled = pending = restartRequired = false;
  sequence = nextTransport = 0; immediate = true;
}
void BleCablePublisher::disconnect() {
  connected = enabled = pending = restartRequired = false; immediate = true;
}
void BleCablePublisher::setEnabled(bool value) {
  value = value && connected;
  if (enabled == value) return;
  enabled = value; pending = false; immediate = true;
}
bool BleCablePublisher::next(uint32_t nowMs, uint32_t nowUs, uint16_t mtu, uint8_t packet[66], uint16_t& length) {
  length = 0;
  if (mode != 0 || !ready) return false;
  if (!clockStarted) {
    clockStarted = true; phase = 1; deadline = nowUs + 33333;
  } else if (int32_t(nowUs - deadline) >= 0) {
    // Advance absolute deadlines, skipping missed slots without catch-up sends.
    deadline += ((nowUs - deadline) / 1000000) * 1000000;
    do {
      deadline += 33333;
      if (++phase == 3) { ++deadline; phase = 0; }
    } while (int32_t(nowUs - deadline) >= 0);
    dirty = true;
  }
  if (!connected || !enabled || restartRequired) return false;
  const uint8_t capacity = mtu >= 69 ? 66 : (mtu >= 23 ? uint8_t(mtu - 3) : 0);
  if (!capacity || (pending && capacity < limit)) {
    restartRequired = true; pending = false; return false;
  }
  if (pending && uint32_t(nowMs - formedAt) >= 100) { pending = false; dirty = true; }
  if (!pending) {
    if (!immediate && !dirty) return false;
    if (!immediate) ++snapshot;
    logical[0] = 2; logical[1] = 0x10; logical[2] = immediate ? 2 : 0; logical[3] = 0;
    put16(logical + 4, sequence++); put32(logical + 6, nowMs);
    put16(logical + 10, 46); put16(logical + 12, epoch);
    put16(logical + 14, snapshot); memcpy(logical + 16, cached, sizeof(cached));
    transport = nextTransport++; limit = capacity; slice = limit - 6;
    fragments = uint8_t((60 + slice - 1) / slice); index = 0;
    formedAt = nowMs; pending = true; immediate = dirty = false;
  }
  const uint16_t offset = uint16_t(index) * slice;
  const uint16_t count = 60 - offset < slice ? 60 - offset : slice;
  put16(packet, transport); packet[2] = index; packet[3] = fragments; put16(packet + 4, 60);
  memcpy(packet + 6, logical + offset, count); length = count + 6;
  return true;
}
void BleCablePublisher::accepted() {
  if (pending && ++index == fragments) pending = false;
}
