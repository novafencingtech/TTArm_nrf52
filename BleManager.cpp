#include "BleManager.h"
#include "BlePairingState.h"
#include "BleGatt.h"
#include "BleStatusPublisher.h"
#include "BleCablePublisher.h"
#include "BleMeasurementPublisher.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#if TTARM_BLE_ENABLED
#include <bluefruit.h>
#include <InternalFileSystem.h>
#include <Adafruit_LittleFS.h>
using namespace Adafruit_LittleFS_Namespace;

namespace {
BlePairingState session;
BleStatusPublisher statusPublisher;
BleCablePublisher cablePublisher;
BleMeasurementPublisher weaponPublisher(0x12), probePublisher(0x13), lightsPublisher(0x14);
BleMeasurementPublisher* measurements[] = {&weaponPublisher, &probePublisher, &lightsPublisher};
const BleGatt::Stream streams[] = {BleGatt::Stream::Weapon, BleGatt::Stream::Probe, BleGatt::Stream::Lights};
SemaphoreHandle_t stateMutex;
bool initialized = false;
bool stopping = false;
bool forgetting = false;
bool forgetQueued = false;
bool disconnectRequested = false;
uint8_t savedBonds = 0;
uint16_t advertisedInterval = 0;
uint32_t lastBondCheck = 0;
uint16_t negotiatedMtu = 23;
bool mtuRequested = false;
uint8_t currentMode = 5; // Idle at boot; modes 0..4 actively sample, 6 is Off.
bool intervalRequestPending = true, intervalRequestAttempted = false;
uint32_t lastIntervalRequest = 0;
bool loopTimingStarted = false;
uint32_t loopWindowStartUs = 0, loopWindowCount = 0, averageLoopUs = 0;
char deviceName[13] = {};
char deviceIdText[17] = {};

// Foreground and BLE event/callback tasks share display state; never an ADC ISR.
struct Lock {
  Lock() { xSemaphoreTake(stateMutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(stateMutex); }
};
uint8_t countBonds() {
  File directory(BOND_DIR_PRPH, FILE_O_READ, InternalFS);
  if (!directory) return 0;
  uint8_t count = 0;
  File entry(InternalFS);
  while ((entry = directory.openNextFile(FILE_O_READ))) {
    if (!entry.isDirectory() && strlen(entry.name()) == 12 && count < 255) ++count;
    entry.close();
  }
  directory.close();
  return count;
}
void disconnectPending() {
  if (disconnectRequested && session.handle != BlePairingState::NoConnection)
    Bluefruit.disconnect(session.handle);
}
void requestMtu(uint16_t handle) {
  if (mtuRequested) return;
  BLEConnection* connection = Bluefruit.Connection(handle);
  if (connection) mtuRequested = connection->requestMtuExchange(TTARM_BLE_TARGET_MTU);
}
void setPreferredInterval() {
  // GAP interval units are 1.25 ms. Keep zero latency and the 4 s timeout.
  if (currentMode == 5) Bluefruit.Periph.setConnInterval(144, 160); // 180..200 ms
  else Bluefruit.Periph.setConnInterval(12, 12); // Request exactly 15 ms.
}
void updateConnectionInterval(uint32_t now) {
  if (!session.encrypted || session.handle == BlePairingState::NoConnection ||
      stopping || forgetting || disconnectRequested || currentMode == 6) return;
  BLEConnection* connection = Bluefruit.Connection(session.handle);
  if (!connection) return;
  const bool tooSlow = currentMode < 5 && connection->getConnectionInterval() > 24;
  if (!intervalRequestPending && !tooSlow) return;
  if (intervalRequestAttempted && uint32_t(now - lastIntervalRequest) < 5000) return;
  lastIntervalRequest = now;
  intervalRequestAttempted = true;
  // Stock SoftDevice API: NULL requests Bluefruit's current preferred range.
  // Returns immediately, including when another link procedure is busy.
  intervalRequestPending = sd_ble_gap_conn_param_update(session.handle, nullptr) != NRF_SUCCESS;
}
void refreshStatusSubscription() {
  statusPublisher.setEnabled(session.handle != BlePairingState::NoConnection && session.encrypted &&
      !stopping && !forgetting && !disconnectRequested && BleGatt::statusSubscribed(session.handle));
}
void refreshCableSubscription() {
  cablePublisher.setEnabled(session.handle != BlePairingState::NoConnection && session.encrypted &&
      !stopping && !forgetting && !disconnectRequested && BleGatt::cableSubscribed(session.handle));
}
void publishCable(uint32_t now) {
  refreshCableSubscription();
  // At most five fragments at MTU 23, no waits for notification credits.
  for (uint8_t attempt = 0; attempt < 5; ++attempt) {
    uint8_t packet[66];
    uint16_t length = 0;
    if (!cablePublisher.next(now, micros(), negotiatedMtu, packet, length)) break;
    const auto sent = BleGatt::tryNotifyCable(session.handle, packet, length);
    if (sent == BleGatt::SendResult::Sent) cablePublisher.accepted();
    else if (sent == BleGatt::SendResult::Retry) break;
    else if (sent == BleGatt::SendResult::Stopped) { cablePublisher.setEnabled(false); break; }
    else { session.fail("Cable link error"); disconnectRequested = true; break; }
  }
  if (cablePublisher.needsReconnect()) {
    session.fail("Cable MTU error"); disconnectRequested = true;
  }
}
void refreshMeasurements() {
  for (uint8_t i = 0; i < 3; ++i)
    measurements[i]->setEnabled(session.handle != BlePairingState::NoConnection && session.encrypted &&
      !stopping && !forgetting && !disconnectRequested && BleGatt::subscribed(session.handle, streams[i]));
}
void publishMeasurements(uint32_t now) {
  refreshMeasurements();
  // Share a bounded credit budget across the active measurement stream.
  for (uint8_t i = 0; i < 3; ++i) {
    auto& publisher = *measurements[i];
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
      uint8_t packet[66]; uint16_t length = 0;
      if (!publisher.next(now, micros(), negotiatedMtu, packet, length)) break;
      const auto sent = BleGatt::tryNotify(session.handle, streams[i], packet, length);
      if (sent == BleGatt::SendResult::Sent) publisher.accepted();
      else if (sent == BleGatt::SendResult::Retry) break;
      else if (sent == BleGatt::SendResult::Stopped) { publisher.setEnabled(false); break; }
      else { session.fail("Measurement link error"); disconnectRequested = true; break; }
    }
    if (publisher.needsReconnect()) { session.fail("Measurement MTU error"); disconnectRequested = true; }
  }
}
void publishStatus(uint32_t now) {
  refreshStatusSubscription();
  BLEConnection* connection = Bluefruit.Connection(session.handle);
  statusPublisher.cacheTiming(averageLoopUs, connection ? connection->getConnectionInterval() : 0);
  // At MTU 23 a Status has three fragments. Bound work per poll, including retries.
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    uint8_t packet[BleStatusPublisher::MaximumPacketLength];
    uint16_t length = 0;
    if (!statusPublisher.next(now, negotiatedMtu, packet, length)) break;
    const auto sent = BleGatt::tryNotifyStatus(session.handle, packet, length);
    if (sent == BleGatt::SendResult::Sent) statusPublisher.accepted();
    else if (sent == BleGatt::SendResult::Retry) break;
    else if (sent == BleGatt::SendResult::Stopped) { statusPublisher.setEnabled(false); break; }
    else { session.fail("Status link error"); disconnectRequested = true; break; }
  }
  if (statusPublisher.needsReconnect()) {
    session.fail("Status MTU error");
    disconnectRequested = true;
  }
}
void onEvent(ble_evt_t* event) {
  Lock lock;
  const uint16_t handle = event->evt.common_evt.conn_handle;
  if (event->header.evt_id == BLE_GAP_EVT_CONNECTED) {
    session.connect(handle, millis());
    statusPublisher.connect();
    cablePublisher.connect();
    for (auto publisher : measurements) publisher->connect();
    negotiatedMtu = 23;
    mtuRequested = false;
    intervalRequestPending = true;
    intervalRequestAttempted = false;
    if (stopping || forgetting) {
      disconnectRequested = true;
      disconnectPending();
    } else {
      requestMtu(handle);
      // Stock pairing: a new phone uses Just Works; a saved phone restores keys.
      BLEConnection* connection = Bluefruit.Connection(handle);
      if (connection && connection->requestPairing()) session.message = "Pairing";
    }
    return;
  }
  if (handle != session.handle) return;
  switch (event->header.evt_id) {
    case BLE_GAP_EVT_DISCONNECTED:
      statusPublisher.disconnect();
      cablePublisher.disconnect();
      for (auto publisher : measurements) publisher->disconnect();
      session.disconnect(handle);
      negotiatedMtu = 23;
      mtuRequested = false;
      disconnectRequested = false;
      advertisedInterval = 0;
      break;
    case BLE_GAP_EVT_CONN_SEC_UPDATE: {
      const auto& security = event->evt.gap_evt.params.conn_sec_update.conn_sec;
      if (security.sec_mode.sm == 1 && security.sec_mode.lv >= 2 && security.encr_key_size >= 7) {
        session.setEncrypted(handle);
        requestMtu(handle); // Retry if the initial request was temporarily busy.
      } else {
        session.fail("Encryption failed");
        disconnectRequested = true;
      }
      break;
    }
    case BLE_GAP_EVT_AUTH_STATUS:
      if (event->evt.gap_evt.params.auth_status.auth_status != BLE_GAP_SEC_STATUS_SUCCESS) {
        session.fail("Pairing failed");
        disconnectRequested = true;
      }
      // The unmodified stack owns saving keys and restoring subscriptions.
      break;
    case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
      negotiatedMtu = event->evt.gatts_evt.params.exchange_mtu_request.client_rx_mtu;
      if (negotiatedMtu > TTARM_BLE_TARGET_MTU) negotiatedMtu = TTARM_BLE_TARGET_MTU;
      mtuRequested = true;
      break;
    case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
      if (event->evt.gattc_evt.gatt_status == BLE_GATT_STATUS_SUCCESS) {
        negotiatedMtu = event->evt.gattc_evt.params.exchange_mtu_rsp.server_rx_mtu;
        if (negotiatedMtu > TTARM_BLE_TARGET_MTU) negotiatedMtu = TTARM_BLE_TARGET_MTU;
        mtuRequested = true;
      }
      break;
    case BLE_GATTS_EVT_WRITE:
      // Observe every accepted CCCD toggle, including off/on between polls.
      refreshMeasurements();
      if (event->evt.gatts_evt.params.write.handle == BleGatt::statusCccdHandle()) refreshStatusSubscription();
      if (event->evt.gatts_evt.params.write.handle == BleGatt::cableCccdHandle()) refreshCableSubscription();
      break;
    default: break;
  }
  refreshStatusSubscription(); // Also handles restored bonded CCCDs after encryption.
  disconnectPending();
}
void finishForget() {
  // Queue on the stock callback worker AFTER disconnect, behind any pending
  // stock bond/CCCD saves. No old save can recreate a record after this clear.
  Bluefruit.Periph.clearBonds();
  const uint8_t remaining = countBonds();
  Lock lock;
  savedBonds = remaining;
  forgetting = forgetQueued = false;
  session.message = remaining == 0 ? "Phones forgotten" : "Forget failed";
}
bool bootRandom(uint64_t& value) {
  const uint32_t started = millis();
  do {
    uint8_t available = 0;
    if (sd_rand_application_bytes_available_get(&available) == NRF_SUCCESS && available >= 8 &&
        sd_rand_application_vector_get(reinterpret_cast<uint8_t*>(&value), 8) == NRF_SUCCESS && value)
      return true;
    delay(1);
  } while (uint32_t(millis() - started) < 500);
  return false;
}
void advertise(uint32_t now) {
  if (session.handle != BlePairingState::NoConnection || stopping || forgetting) return;
  const uint16_t interval = session.windowOpen ? (session.secondsLeft(now) > 30 ? 32 : 244) : 874;
  if (Bluefruit.Advertising.isRunning() && advertisedInterval == interval) return;
  Bluefruit.Advertising.stop();
  Bluefruit.Advertising.setInterval(interval, interval); // 0.625 ms units
  Bluefruit.Advertising.setFastTimeout(0);
  if (!Bluefruit.Advertising.start(0)) {
    session.message = "Advertise error";
    advertisedInterval = 0;
  } else advertisedInterval = interval;
}
}

bool BleManager::begin(uint16_t revision) {
  if (stateMutex) return initialized;
  stateMutex = xSemaphoreCreateMutex();
  if (!stateMutex) return false;
  const uint64_t deviceId = (uint64_t(NRF_FICR->DEVICEID[1]) << 32) | NRF_FICR->DEVICEID[0];
  snprintf(deviceIdText, sizeof(deviceIdText), "%08lX%08lX",
           static_cast<unsigned long>(deviceId >> 32), static_cast<unsigned long>(deviceId));
  snprintf(deviceName, sizeof(deviceName), "TTArm-%06lX", static_cast<unsigned long>(deviceId & 0xffffff));
  Bluefruit.autoConnLed(false);
  Bluefruit.configServiceChanged(true);
  // Runtime stack configuration; no core patch, linker or compiler override.
  Bluefruit.configPrphConn(TTARM_BLE_TARGET_MTU, BLE_GAP_EVENT_LENGTH_DEFAULT, 1, 1);
  Bluefruit.Security.setIOCaps(false, false, false);
  Bluefruit.Security.setMITM(false); // Just Works on both 832 and 840; no passkey.
  if (!deviceId || !Bluefruit.begin(1, 0)) { session.message = "BLE init failed"; return false; }
  uint64_t bootId = 0;
  if (!bootRandom(bootId) || !BleGatt::begin(deviceId, bootId, revision, TTARM_BLE_BUILD)) {
    session.message = "BLE GATT failed";
    return false;
  }
  Bluefruit.setName(deviceName);
  Bluefruit.setEventCallback(onEvent);
  setPreferredInterval();
  Bluefruit.Periph.setConnSlaveLatency(0);
  Bluefruit.Periph.setConnSupervisionTimeoutMS(4000);
  Bluefruit.Advertising.restartOnDisconnect(false);
  Bluefruit.Advertising.clearData();
  Bluefruit.ScanResponse.clearData();
  if (!Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE) ||
      !Bluefruit.Advertising.addUuid(BleGatt::ServiceUuid) || !Bluefruit.ScanResponse.addName()) {
    session.message = "BLE advert failed";
    return false;
  }
  savedBonds = countBonds();
  initialized = true;
  poll(millis());
  return true;
}
void BleManager::recordLoopStart(uint32_t nowUs) {
  if (!initialized) return;
  Lock lock;
  if (!loopTimingStarted) {
    loopTimingStarted = true;
    loopWindowStartUs = nowUs;
    return;
  }
  ++loopWindowCount;
  const uint32_t elapsedUs = nowUs - loopWindowStartUs; // Wrap-safe micros delta.
  if (elapsedUs >= 1000000) {
    averageLoopUs = elapsedUs / loopWindowCount;
    loopWindowStartUs = nowUs;
    loopWindowCount = 0;
  }
}
void BleManager::poll(uint32_t now) {
  if (!initialized) return;
  Lock lock;
  if (session.expired(now)) session.cancel();
  if (session.connectionExpired(now)) {
    session.fail("Pairing timed out");
    disconnectRequested = true;
  }
  disconnectPending();
  if (forgetting && session.handle == BlePairingState::NoConnection && !forgetQueued)
    forgetQueued = ada_callback(nullptr, 0, finishForget);
  if (!forgetting && uint32_t(now - lastBondCheck) >= 1000) {
    savedBonds = countBonds(); // Stock saves are asynchronous; show actual files.
    lastBondCheck = now;
  }
  advertise(now);
  updateConnectionInterval(now);
  publishStatus(now);
  publishCable(now);
  publishMeasurements(now);
  disconnectPending();
}
bool BleManager::startPairing(uint32_t now) {
  if (!initialized) return false;
  Lock lock;
  return !stopping && !forgetting && session.open(now);
}
void BleManager::cancelPairing() {
  if (!initialized) return;
  Lock lock;
  session.cancel();
  if (!session.encrypted && session.handle != BlePairingState::NoConnection) disconnectRequested = true;
  disconnectPending();
}
void BleManager::forgetBonds() {
  if (!initialized) return;
  Lock lock;
  session.cancel();
  session.message = "Forgetting phones";
  forgetting = true;
  Bluefruit.Advertising.stop();
  disconnectRequested = session.handle != BlePairingState::NoConnection;
  disconnectPending();
}
void BleManager::stop() {
  if (!initialized) return;
  Lock lock;
  publishStatus(millis()); // Bounded best-effort pre-shutdown Status; never wait.
  statusPublisher.disconnect();
  cablePublisher.disconnect();
  for (auto publisher : measurements) publisher->disconnect();
  session.cancel();
  stopping = true;
  session.message = "BLE stopped";
  Bluefruit.Advertising.stop();
  disconnectRequested = session.handle != BlePairingState::NoConnection;
  disconnectPending();
}
BleManager::Status BleManager::getStatus(uint32_t now) {
  Status out = {};
  out.message = "BLE init failed";
  if (!stateMutex) return out;
  Lock lock;
  out.available = initialized && !stopping;
  out.connected = session.handle != BlePairingState::NoConnection;
  out.encrypted = session.encrypted;
  out.secondsLeft = session.secondsLeft(now);
  out.pairing = out.secondsLeft > 0;
  out.bonds = savedBonds;
  out.generation = session.generation;
  out.mtu = out.connected ? negotiatedMtu : 0;
  memcpy(out.name, deviceName, sizeof(out.name));
  memcpy(out.deviceId, deviceIdText, sizeof(out.deviceId));
  out.message = session.message;
  return out;
}
void BleManager::cacheDiagnostics(uint32_t count, uint32_t roundedRateHz) {
  if (!stateMutex) return;
  Lock lock;
  statusPublisher.cacheDiagnostics(count, roundedRateHz);
}
void BleManager::cacheBattery(float volts) {
  if (!stateMutex) return;
  Lock lock;
  statusPublisher.cacheBattery(volts);
}
void BleManager::setMode(uint8_t protocolMode) {
  if (!stateMutex) return;
  Lock lock;
  if (protocolMode <= 6 && protocolMode != currentMode) {
    currentMode = protocolMode;
    setPreferredInterval();
    intervalRequestPending = true;
    intervalRequestAttempted = false; // A mode change gets an immediate attempt.
    updateConnectionInterval(millis());
  }
  statusPublisher.setMode(protocolMode);
  if (protocolMode <= 6) {
    cablePublisher.setMode(protocolMode, statusPublisher.epoch());
    for (auto publisher : measurements) publisher->setMode(protocolMode, statusPublisher.epoch());
  }
}
void BleManager::cacheWeapon(uint8_t flags, float e, float f, uint32_t ee, uint32_t fe, float open) {
  if (!stateMutex) return;
  Lock lock;
  weaponPublisher.cacheWeapon(flags, e, f, ee, fe, open);
}
void BleManager::cacheProbe(const float values[6], float open) {
  if (!stateMutex) return;
  Lock lock;
  probePublisher.cacheProbe(values, open);
}
void BleManager::cacheLights(uint8_t flags, uint32_t ee, uint32_t fe, uint32_t now) {
  if (!stateMutex) return;
  Lock lock;
  lightsPublisher.cacheLights(flags, ee, fe, now);
}
void BleManager::cacheCable(uint16_t faults, const float resistance[6], const int adc[9], float open) {
  if (!stateMutex) return;
  Lock lock;
  cablePublisher.cache(faults, resistance, adc, open);
}
#else
void BleManager::cacheWeapon(uint8_t, float, float, uint32_t, uint32_t, float) {}
void BleManager::cacheProbe(const float[6], float) {}
void BleManager::cacheLights(uint8_t, uint32_t, uint32_t, uint32_t) {}
void BleManager::cacheCable(uint16_t, const float[6], const int[9], float) {}
bool BleManager::begin(uint16_t) { return false; }
void BleManager::poll(uint32_t) {}
void BleManager::recordLoopStart(uint32_t) {}
bool BleManager::startPairing(uint32_t) { return false; }
void BleManager::cancelPairing() {}
void BleManager::forgetBonds() {}
void BleManager::stop() {}
void BleManager::cacheDiagnostics(uint32_t, uint32_t) {}
void BleManager::cacheBattery(float) {}
void BleManager::setMode(uint8_t) {}
BleManager::Status BleManager::getStatus(uint32_t) {
  Status out = {};
  out.message = "BLE disabled";
  return out;
}
#endif
