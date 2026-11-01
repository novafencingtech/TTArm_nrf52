#include "BleStatusPublisher.h"
#include <string.h>

namespace {
void put16(uint8_t* out, uint16_t value) {
  out[0] = uint8_t(value); out[1] = uint8_t(value >> 8);
}
void put32(uint8_t* out, uint32_t value) {
  for (uint8_t i = 0; i < 4; ++i) out[i] = uint8_t(value >> (8 * i));
}
uint8_t capacity(uint16_t mtu) { return mtu >= 69 ? 66 : uint8_t(mtu - 3); }
}

void BleStatusPublisher::cacheDiagnostics(uint32_t count, uint32_t rateHz) {
  diagnosticCount = count;
  diagnosticRate = rateHz;
  diagnosticsReady = true;
}

void BleStatusPublisher::cacheTiming(uint32_t loopUs, uint16_t intervalUnits) {
  averageLoopUs = loopUs;
  connectionIntervalUnits = intervalUnits;
}

bool BleStatusPublisher::cacheBattery(float volts) {
  // Comparisons also reject NaN/infinity before converting to an integer.
  if (!(volts >= 0 && volts <= 65.535f)) return false;
  const uint32_t rounded = uint32_t(volts * 1000.0f + 0.5f);
  if (rounded > 65534) return false;
  batteryMv = uint16_t(rounded);
  batteryReady = true;
  return true;
}

void BleStatusPublisher::setMode(uint8_t mode) {
  if (mode > 6 || mode == currentMode) return;
  currentMode = mode;
  ++modeEpoch;
  pending = false; // Never finish an old-mode fragment after a transition.
  immediate = true;
}

void BleStatusPublisher::connect() {
  connected = true;
  enabled = false;
  immediate = true;
  formed = pending = restartRequired = false;
  nextSequence = nextTransport = 0;
  // Boot-local mode/epoch and initialized diagnostics survive reconnects.
}

void BleStatusPublisher::disconnect() {
  connected = enabled = pending = false;
  immediate = true;
  restartRequired = false;
}

void BleStatusPublisher::setEnabled(bool value) {
  value = value && connected;
  if (value == enabled) return;
  enabled = value;
  pending = false;
  immediate = true;
}

void BleStatusPublisher::form(uint32_t now, uint8_t limit) {
  logical[0] = 2;
  logical[1] = 0x01;
  logical[2] = immediate ? 0x02 : 0;
  logical[3] = currentMode;
  put16(logical + 4, nextSequence++);
  put32(logical + 6, now);
  put16(logical + 10, 16);
  put16(logical + 12, modeEpoch);
  put32(logical + 14, diagnosticCount);
  put32(logical + 18, diagnosticRate);
  put16(logical + 22, batteryMv);
  put32(logical + 24, averageLoopUs);
  put16(logical + 28, connectionIntervalUnits);
  transport = nextTransport++;
  valueLimit = limit;
  sliceLength = limit - 6;
  fragmentCount = uint8_t((LogicalLength + sliceLength - 1) / sliceLength);
  fragmentIndex = 0;
  formedAt = now;
  formed = pending = true;
  immediate = false;
}

bool BleStatusPublisher::next(uint32_t now, uint16_t mtu, uint8_t packet[MaximumPacketLength],
                               uint16_t& length) {
  length = 0;
  if (!connected || !enabled || !ready() || restartRequired) return false;
  if (mtu < 23 || (pending && capacity(mtu) < valueLimit)) {
    restartRequired = true; // Do not change boundaries of a pending frame.
    pending = false;
    return false;
  }
  if (pending && uint32_t(now - formedAt) >= 2000) pending = false;
  if (!pending) {
    if (!immediate && formed && uint32_t(now - formedAt) < 1000) return false;
    form(now, capacity(mtu));
  }
  const uint16_t offset = uint16_t(fragmentIndex) * sliceLength;
  const uint16_t remaining = LogicalLength - offset;
  const uint16_t count = remaining < sliceLength ? remaining : sliceLength;
  put16(packet, transport);
  packet[2] = fragmentIndex;
  packet[3] = fragmentCount;
  put16(packet + 4, LogicalLength);
  memcpy(packet + 6, logical + offset, count);
  length = count + 6;
  return true;
}

void BleStatusPublisher::accepted() {
  if (pending && ++fragmentIndex == fragmentCount) pending = false;
}
