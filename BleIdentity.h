#ifndef TTARM_BLE_IDENTITY_H
#define TTARM_BLE_IDENTITY_H
#include <stdint.h>
namespace BleIdentity {
static const uint8_t PublicProtocol[8] = {0x54, 0x54, 0x42, 0x4c, 2, 1, 1, 0};
void encode(uint8_t out[68], uint64_t deviceId, uint64_t bootId, uint16_t revision, const char* build);
}
#endif
