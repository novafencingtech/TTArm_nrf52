#ifndef TTARM_BLE_PAIRING_STATE_H
#define TTARM_BLE_PAIRING_STATE_H
#include <stdint.h>

// Tracks discovery and actual link encryption. Pairing/bonds belong to Bluefruit.
class BlePairingState {
public:
  static const uint16_t NoConnection = 0xffff;
  static const uint32_t WindowMs = 60000;
  static const uint32_t ConnectionTimeoutMs = 60000;
  uint16_t handle = NoConnection;
  uint32_t generation = 0;
  bool windowOpen = false;
  bool encrypted = false;
  const char* message = "Ready";
  bool open(uint32_t now);
  void connect(uint16_t connection, uint32_t now);
  void disconnect(uint16_t connection);
  void setEncrypted(uint16_t connection);
  void cancel();
  void fail(const char* reason);
  bool expired(uint32_t now) const;
  bool connectionExpired(uint32_t now) const;
  uint8_t secondsLeft(uint32_t now) const;
private:
  bool failed = false;
  uint32_t openedAt = 0;
  uint32_t connectedAt = 0;
};
#endif
