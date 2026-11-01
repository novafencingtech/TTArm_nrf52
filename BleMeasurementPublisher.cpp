#include "BleMeasurementPublisher.h"
#include <string.h>
#include <math.h>
namespace {
void put16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }
void put32(uint8_t* p, uint32_t v) { for (uint8_t i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8*i)); }
bool resistance(uint8_t* p, float v, float open) {
  if (!isfinite(v) || !isfinite(open)) return false;
  int32_t value = INT32_MAX;
  if (v < open) {
    double scaled = double(v) * 1000.0;
    double rounded = scaled < 0 ? ceil(scaled - 0.5) : floor(scaled + 0.5);
    if (rounded < -2147483647.0 || rounded > 2147483646.0) return false;
    value = int32_t(rounded);
  }
  put32(p, uint32_t(value)); return true;
}
}
bool BleMeasurementPublisher::active() const {
  return (type == 0x12 && (mode == 2 || mode == 3)) ||
         (type == 0x13 && mode == 4) || (type == 0x14 && mode == 1);
}
void BleMeasurementPublisher::setMode(uint8_t value, uint16_t generation) {
  if (mode == value && epoch == generation) return;
  mode = value; epoch = generation;
  ready = pending = clockStarted = dirty = false; immediate = true; head = count = 0;
}
void BleMeasurementPublisher::connect() {
  connected = true; enabled = pending = restartRequired = false;
  sequence = nextTransport = 0; immediate = true; head = count = 0;
}
void BleMeasurementPublisher::disconnect() {
  connected = enabled = pending = restartRequired = false; immediate = true; head = count = 0;
}
void BleMeasurementPublisher::setEnabled(bool value) {
  value = value && connected;
  if (enabled == value) return;
  enabled = value; pending = false; immediate = true; head = count = 0;
}
void BleMeasurementPublisher::cacheWeapon(uint8_t flags, float e, float f, uint32_t ee, uint32_t fe, float open) {
  if (type != 0x12 || !active()) return;
  uint8_t bytes[17] = {}; bytes[0] = flags;
  if ((flags & 0xe0) || !resistance(bytes+1, e, open) || !resistance(bytes+5, f, open)) {
    ready = pending = false; return;
  }
  put32(bytes+9, ee); put32(bytes+13, fe);
  memcpy(cached, bytes, sizeof(bytes)); ready = true;
}
void BleMeasurementPublisher::cacheProbe(const float values[6], float open) {
  if (type != 0x13 || !active()) return;
  uint8_t bytes[24];
  for (uint8_t i = 0; i < 6; ++i) if (!resistance(bytes+4*i, values[i], open)) {
    ready = pending = false; return;
  }
  memcpy(cached, bytes, sizeof(bytes)); ready = true;
}
void BleMeasurementPublisher::cacheLights(uint8_t flags, uint32_t ee, uint32_t fe, uint32_t now) {
  if (type != 0x14 || !active() || (flags & 0xf0)) return;
  uint8_t bytes[10] = {}; bytes[0] = flags;
  put32(bytes+2, ee); put32(bytes+6, fe);
  const bool changed = ready && memcmp(cached, bytes, sizeof(bytes)) != 0;
  memcpy(cached, bytes, sizeof(bytes)); ready = true;
  if (!changed) return;
  ++snapshot;
  if (!enabled || immediate) return; // Immediate copy will contain the latest full state.
  if (count == 16) { head = count = 0; } // Discard old states on overload.
  State& state = queue[(head + count) % 16];
  memcpy(state.bytes, bytes, sizeof(bytes)); state.snapshot = snapshot; state.time = now; ++count;
}
bool BleMeasurementPublisher::next(uint32_t now, uint32_t nowUs, uint16_t mtu, uint8_t packet[66], uint16_t& length) {
  length = 0;
  if (!active() || !ready) return false;
  if (type != 0x14) {
    if (!clockStarted) { clockStarted = true; phase = 1; deadline = nowUs + 33333; }
    else if (int32_t(nowUs - deadline) >= 0) {
      deadline += ((nowUs - deadline) / 1000000) * 1000000;
      do { deadline += 33333; if (++phase == 3) { ++deadline; phase = 0; } }
      while (int32_t(nowUs - deadline) >= 0);
      dirty = true;
    }
  }
  if (!connected || !enabled || restartRequired) return false;
  const uint8_t capacity = mtu >= 69 ? 66 : (mtu >= 23 ? uint8_t(mtu-3) : 0);
  if (!capacity || (pending && capacity < limit)) {
    restartRequired = true; pending = false; return false;
  }
  if (pending && uint32_t(now - formedAt) >= 100) { pending = false; dirty = true; }
  // Expired changes collapse to the newest full state, never delayed playback.
  if (count && uint32_t(now - queue[head].time) >= 100) {
    head = count = 0; dirty = true;
  }
  if (!pending) {
    const bool lights = type == 0x14;
    if (!immediate && !dirty && !(lights && (count || uint32_t(now-refreshAt) >= 1000))) return false;
    const uint8_t dataLength = lights ? 10 : (type == 0x12 ? 17 : 24);
    const uint8_t* data = cached;
    uint16_t id = snapshot; uint32_t timestamp = now;
    bool urgent = immediate;
    if (lights && count && !immediate) {
      const State& state = queue[head]; data = state.bytes; id = state.snapshot; timestamp = state.time;
      head = (head+1) % 16; --count; urgent = true;
    } else if (!lights && !immediate) id = ++snapshot;
    logical[0] = 2; logical[1] = type; logical[2] = urgent ? 2 : 0; logical[3] = mode;
    put16(logical+4, sequence++); put32(logical+6, timestamp);
    put16(logical+10, dataLength+2); put16(logical+12, epoch); put16(logical+14, id);
    memcpy(logical+16, data, dataLength); logicalLength = dataLength+16;
    transport = nextTransport++; limit = capacity; slice = limit-6;
    fragments = (logicalLength+slice-1)/slice; index = 0;
    formedAt = timestamp; refreshAt = now; pending = true; immediate = dirty = false;
  }
  const uint16_t offset = uint16_t(index)*slice;
  const uint16_t size = logicalLength-offset < slice ? logicalLength-offset : slice;
  put16(packet, transport); packet[2] = index; packet[3] = fragments; put16(packet+4, logicalLength);
  memcpy(packet+6, logical+offset, size); length = size+6; return true;
}
void BleMeasurementPublisher::accepted() { if (pending && ++index == fragments) pending = false; }
