#ifndef TTARM_BLE_MEASUREMENT_PUBLISHER_H
#define TTARM_BLE_MEASUREMENT_PUBLISHER_H
#include <stdint.h>

// Foreground-only cache/transport; BleManager holds its state mutex.
class BleMeasurementPublisher {
public:
  explicit BleMeasurementPublisher(uint8_t type) : type(type) {}
  void setMode(uint8_t value, uint16_t generation);
  void connect();
  void disconnect();
  void setEnabled(bool value);
  void cacheWeapon(uint8_t flags, float epee, float foil, uint32_t eEdge, uint32_t fEdge, float open);
  void cacheProbe(const float resistance[6], float open);
  void cacheLights(uint8_t flags, uint32_t eEdge, uint32_t fEdge, uint32_t now);
  bool next(uint32_t now, uint32_t nowUs, uint16_t mtu, uint8_t packet[66], uint16_t& length);
  void accepted();
  bool needsReconnect() const { return restartRequired; }
private:
  struct State { uint8_t bytes[10]; uint16_t snapshot; uint32_t time; };
  State queue[16] = {};
  uint8_t cached[24] = {}, logical[40] = {};
  uint8_t type, mode = 5, head = 0, count = 0, phase = 0;
  uint8_t limit = 20, slice = 14, index = 0, fragments = 0, logicalLength = 0;
  uint16_t epoch = 0, snapshot = 0, sequence = 0, nextTransport = 0, transport = 0;
  uint32_t deadline = 0, refreshAt = 0, formedAt = 0;
  bool ready = false, clockStarted = false, dirty = false, immediate = true;
  bool connected = false, enabled = false, pending = false, restartRequired = false;
  bool active() const;
};
#endif
