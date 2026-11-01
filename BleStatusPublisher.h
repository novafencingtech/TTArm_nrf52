#ifndef TTARM_BLE_STATUS_PUBLISHER_H
#define TTARM_BLE_STATUS_PUBLISHER_H
#include <stdint.h>

// One immutable Status frame in flight. No allocation, SDK calls or ADC access.
// The manager serializes all calls with its existing mutex.
class BleStatusPublisher {
public:
  static const uint16_t LogicalLength = 30;
  static const uint16_t MaximumPacketLength = 36;
  void cacheDiagnostics(uint32_t count, uint32_t rateHz);
  void cacheTiming(uint32_t averageLoopUs, uint16_t connectionIntervalUnits);
  bool cacheBattery(float volts);
  bool ready() const { return diagnosticsReady && batteryReady; }
  void setMode(uint8_t mode);
  uint16_t epoch() const { return modeEpoch; }
  void connect();
  void disconnect();
  void setEnabled(bool enabled);
  bool next(uint32_t now, uint16_t mtu, uint8_t packet[MaximumPacketLength], uint16_t& length);
  void accepted();
  bool needsReconnect() const { return restartRequired; }

private:
  uint32_t diagnosticCount = 0, diagnosticRate = 0;
  uint32_t averageLoopUs = 0;
  uint16_t connectionIntervalUnits = 0;
  uint16_t batteryMv = 0;
  bool diagnosticsReady = false, batteryReady = false;
  uint8_t currentMode = 5; // The sketch boots in Idle.
  uint16_t modeEpoch = 0;
  bool connected = false, enabled = false, immediate = true;
  bool formed = false, pending = false, restartRequired = false;
  uint32_t formedAt = 0;
  uint16_t nextSequence = 0, nextTransport = 0, transport = 0;
  uint8_t logical[LogicalLength] = {};
  uint8_t valueLimit = 20, sliceLength = 14, fragmentIndex = 0, fragmentCount = 0;
  void form(uint32_t now, uint8_t limit);
};
#endif
