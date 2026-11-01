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
  uint16_t mtu; // Actual negotiated ATT MTU; zero when disconnected.
  char name[13];
  char deviceId[17];
  const char* message;
};
bool begin(uint16_t hardwareRevision);
void poll(uint32_t now);
void recordLoopStart(uint32_t nowUs); // Call once at the start of the main loop.
bool startPairing(uint32_t now);
void cancelPairing();
void forgetBonds(); // Only after explicit on-device confirmation.
void stop();
Status getStatus(uint32_t now);
void cacheDiagnostics(uint32_t count, uint32_t roundedRateHz);
void cacheBattery(float volts);
void setMode(uint8_t protocolMode);
void cacheWeapon(uint8_t flags, float epee, float foil, uint32_t eEdge, uint32_t fEdge, float open);
void cacheProbe(const float resistance[6], float open);
void cacheLights(uint8_t flags, uint32_t eEdge, uint32_t fEdge, uint32_t now);
void cacheCable(uint16_t faults, const float resistance[6], const int adc[9], float open);
}
#endif
