#ifndef TTARM_BLE_GATT_H
#define TTARM_BLE_GATT_H
#include <stdint.h>
namespace BleGatt {
extern const char ServiceUuid[];
bool begin(uint64_t deviceId, uint64_t bootId, uint16_t revision, const char* build);
}
#endif
