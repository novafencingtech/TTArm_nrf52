#ifndef TTARM_BLE_GATT_H
#define TTARM_BLE_GATT_H
#include <stdint.h>
namespace BleGatt {
extern const char ServiceUuid[];
bool begin(uint64_t deviceId, uint64_t bootId, uint16_t revision, const char* build);
enum class SendResult { Sent, Retry, Stopped, Restart };
enum class Stream { Weapon, Probe, Lights };
uint16_t cccdHandle(Stream stream);
bool subscribed(uint16_t connection, Stream stream);
SendResult tryNotify(uint16_t connection, Stream stream, const uint8_t* packet, uint16_t length);
uint16_t cableCccdHandle();
bool cableSubscribed(uint16_t connection);
SendResult tryNotifyCable(uint16_t connection, const uint8_t* packet, uint16_t length);
uint16_t statusCccdHandle();
bool statusSubscribed(uint16_t connection);
SendResult tryNotifyStatus(uint16_t connection, const uint8_t* packet, uint16_t length);
}
#endif
