#include "BlePairingState.h"

bool BlePairingState::open(uint32_t now) {
  if (handle != NoConnection) { message = "Phone connected"; return false; }
  openedAt = now;
  failed = false;
  windowOpen = true;
  message = "Select on phone";
  return true;
}
void BlePairingState::connect(uint16_t connection, uint32_t now) {
  handle = connection;
  ++generation;
  connectedAt = now;
  encrypted = false;
  failed = false;
  message = "Connected";
}
void BlePairingState::disconnect(uint16_t connection) {
  if (handle != connection) return;
  handle = NoConnection;
  ++generation;
  encrypted = false;
  if (!failed) message = "Disconnected";
}
void BlePairingState::setEncrypted(uint16_t connection) {
  if (handle != connection || connection == NoConnection) return;
  encrypted = true;
  windowOpen = false;
  message = "Encrypted";
}
void BlePairingState::cancel() {
  windowOpen = false;
  if (handle == NoConnection) message = "Ready";
}
void BlePairingState::fail(const char* reason) {
  failed = true;
  windowOpen = encrypted = false;
  message = reason;
}
bool BlePairingState::expired(uint32_t now) const {
  return windowOpen && uint32_t(now - openedAt) >= WindowMs;
}
bool BlePairingState::connectionExpired(uint32_t now) const {
  return handle != NoConnection && !encrypted &&
         uint32_t(now - connectedAt) >= ConnectionTimeoutMs;
}
uint8_t BlePairingState::secondsLeft(uint32_t now) const {
  if (!windowOpen || expired(now)) return 0;
  return uint8_t((WindowMs - uint32_t(now - openedAt) + 999) / 1000);
}
