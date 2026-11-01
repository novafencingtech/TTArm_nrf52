#include "BleIdentity.h"
#include <string.h>
namespace {
void put16(uint8_t* out, uint16_t value) {
  out[0] = uint8_t(value); out[1] = uint8_t(value >> 8);
}
void put64(uint8_t* out, uint64_t value) {
  for (uint8_t i = 0; i < 8; ++i) out[i] = uint8_t(value >> (8 * i));
}
}
void BleIdentity::encode(uint8_t out[68], uint64_t deviceId, uint64_t bootId,
                         uint16_t revision, const char* build) {
  memset(out, 0, 68);
  out[0] = 2; out[1] = 1; out[2] = 1; out[3] = 68;
  put64(out + 4, deviceId); put64(out + 12, bootId);
  // Complete Cable stream plus battery diagnostics; other streams/commands deferred.
  out[20] = 0x01; // Capability bit 0: combined Cable stream.
  out[21] = 0x02; // Capability bit 9: battery millivolt acquisition (0x200).
  put16(out + 24, 1); put16(out + 26, revision);
  out[28] = 1; out[29] = 2; out[30] = 1; // Firmware 1.2.1, BLE development build.
  if (build) strncpy(reinterpret_cast<char*>(out + 32), build, 23);
  put16(out + 56, 512);
  out[58] = 66; out[59] = 30; out[64] = 2; // Standard encrypted Just Works pairing.
}
