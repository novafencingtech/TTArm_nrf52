#ifndef TTARM_BLE_CABLE_PUBLISHER_H
#define TTARM_BLE_CABLE_PUBLISHER_H
#include <stdint.h>

// Foreground cache and bounded, immutable Cable transmission. Manager owns lock.
class BleCablePublisher {
public:
  void cache(uint16_t faults, const float resistance[6], const int adc[9], float open);
  void setMode(uint8_t mode, uint16_t epoch);
  void connect();
  void disconnect();
  void setEnabled(bool value);
  bool next(uint32_t nowMs, uint32_t nowUs, uint16_t mtu, uint8_t packet[66], uint16_t& length);
  void accepted();
  bool needsReconnect() const { return restartRequired; }
private:
  uint8_t cached[44] = {}, logical[60] = {};
  uint8_t mode = 5, phase = 0, limit = 20, slice = 14, index = 0, fragments = 0;
  uint16_t epoch = 0, snapshot = 0, sequence = 0, nextTransport = 0, transport = 0;
  uint32_t deadline = 0, formedAt = 0;
  bool ready = false, clockStarted = false, dirty = false, immediate = true;
  bool connected = false, enabled = false, pending = false, restartRequired = false;
};
#endif
