#ifndef TTARM_BLE_MANAGER_H
#define TTARM_BLE_MANAGER_H
#include "BleConfig.h"
#include <stdint.h>
namespace BleManager {
struct Status {
  bool available;
  bool connected;
  bool encrypted;
  bool pairing;
  uint8_t secondsLeft;
  uint8_t bonds;
  uint32_t generation;
  char name[13];
  char deviceId[17];
  const char* message;
};
bool begin(uint16_t hardwareRevision);
void poll(uint32_t now);
bool startPairing(uint32_t now);
void cancelPairing();
void forgetBonds(); // Only after explicit on-device confirmation.
void stop();
Status getStatus(uint32_t now);
}
#endif
