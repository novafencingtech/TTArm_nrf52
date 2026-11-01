#include "BlePairingState.h"
#include "BleIdentity.h"
#include "BleGatt.h"
#include "BleManager.h"
#include "BleUi.h"
#include "MenuSystem.h"
#include <bluefruit.h>
#include <cassert>
#include <cstring>
#include <iostream>

uint32_t testMillis = 100;
static BleManager::Status uiStatus = {};
BleManager::Status BleManager::getStatus(uint32_t) { return uiStatus; }
static int pairActions = 0, cancelActions = 0, forgetActions = 0, calibrations = 0;
extern "C" void calibrateSystem() { ++calibrations; }

static void pairingPolicy() {
  BlePairingState s;
  assert(s.open(100)); assert(s.secondsLeft(100) == 60);
  assert(!s.expired(60099) && s.expired(60100));
  s.cancel(); assert(!s.windowOpen);
  // Connection/encryption do not require the fast-discovery window.
  s.connect(1, 1000); assert(!s.encrypted);
  assert(!s.open(1001)); // Do not replace a connected phone.
  assert(!s.connectionExpired(60999) && s.connectionExpired(61000));
  s.setEncrypted(99); assert(!s.encrypted);
  s.setEncrypted(1); assert(s.encrypted && !s.windowOpen);
  assert(!s.connectionExpired(999999));
  auto generation = s.generation;
  s.disconnect(99); assert(s.encrypted && s.generation == generation);
  s.disconnect(1); assert(!s.encrypted && s.generation != generation);
  assert(s.open(2000)); s.connect(2, 2001); s.cancel();
  assert(s.handle == 2 && !s.windowOpen); // Manager decides whether to disconnect.
  s.fail("Pairing failed"); assert(!s.encrypted && !s.windowOpen);
  s.disconnect(2);
  assert(s.open(3000)); // No artificial capacity/cooldown restriction.
  BlePairingState wrap;
  const uint32_t start = 0xfffffff0;
  assert(wrap.open(start)); wrap.connect(1, start);
  assert(wrap.secondsLeft(start) == 60);
  assert(!wrap.expired(start + uint32_t(59999)));
  assert(wrap.expired(start + uint32_t(60000)));
  assert(wrap.connectionExpired(start + uint32_t(60000)));
  assert(wrap.secondsLeft(start + uint32_t(60000)) == 0);
}

static void identityAndGatt() {
  uint8_t bytes[68];
  BleIdentity::encode(bytes, 0x0102030405060708ULL, 0x1112131415161718ULL, 2, "ble1");
  assert(bytes[0] == 2 && bytes[1] == 0 && bytes[2] == 1 && bytes[3] == 68);
  for (int i = 0; i < 8; ++i) {
    assert(bytes[4 + i] == 8 - i); assert(bytes[12 + i] == 0x18 - i);
  }
  assert(bytes[20] == 1 && bytes[21] == 2 && bytes[22] == 0 && bytes[23] == 0);
  assert(bytes[24] == 1 && bytes[26] == 2 && bytes[31] == 0);
  assert(std::strcmp(reinterpret_cast<char*>(bytes + 32), "ble1") == 0);
  for (int i = 36; i < 56; ++i) assert(bytes[i] == 0);
  assert(bytes[56] == 0 && bytes[57] == 2 && bytes[58] == 66 && bytes[59] == 30 && bytes[64] == 2);
  for (int i : {60,61,62,63,65,66,67}) assert(bytes[i] == 0);
  BleIdentity::encode(bytes, 1, 2, 0, "123456789012345678901234567890");
  assert(bytes[55] == 0); // Text cannot consume the terminator or following fields.
  assert(BleGatt::begin(1, 2, 2, "ble1"));
  auto& list = BLECharacteristic::all();
  assert(list.size() == 10);
  assert(list[0]->value.size() == 8 && std::memcmp(list[0]->value.data(), BleIdentity::PublicProtocol, 8) == 0);
  assert(list[0]->readPermission == SECMODE_OPEN);
  assert(list[1]->value.size() == 68 && list[1]->readPermission == SECMODE_ENC_NO_MITM);
  const char* roles[] = {"0001","0002","0010","0011","0012","0013","0014","0020","0030","0031"};
  for (size_t i = 0; i < list.size(); ++i) {
    assert(list[i]->uuid == std::string("4a1e") + roles[i] + "-3bb5-44b4-b566-8d22e72da6f5");
    assert(list[i]->writePermission == SECMODE_NO_ACCESS);
    if (i >= 2 && i <= 7) {
      assert(list[i]->properties == CHR_PROPS_NOTIFY && list[i]->maxLength == 66);
      assert(list[i]->readPermission == SECMODE_ENC_NO_MITM && list[i]->value.empty());
    }
  }
  assert(list[8]->properties == CHR_PROPS_WRITE && list[8]->maxLength == 9);
  assert(list[9]->properties == CHR_PROPS_INDICATE && list[9]->maxLength == 10 && list[9]->value.empty());
}

static bool hasText(const Adafruit_SSD1351& display, const char* text) {
  for (const auto& item : display.text) if (item.value == text) return true;
  return false;
}
static void action(MenuSystem::BluetoothAction value) {
  if (value == MenuSystem::PAIR_PHONE) ++pairActions;
  if (value == MenuSystem::CANCEL_PAIRING) ++cancelActions;
  if (value == MenuSystem::FORGET_PHONES) ++forgetActions;
}
static void tap(MenuSystem& menu) {
  menu.updateMenu(true, ++testMillis); menu.updateMenu(false, ++testMillis);
}
static void hold(MenuSystem& menu) {
  menu.updateMenu(true, ++testMillis); testMillis += 1000;
  menu.updateMenu(true, testMillis); menu.updateMenu(false, ++testMillis);
}
static void menuAndUi() {
  uiStatus.available = true; uiStatus.message = "Select on phone"; uiStatus.pairing = true;
  uiStatus.secondsLeft = 60;
  std::strcpy(uiStatus.name, "TTArm-A1B2C3"); std::strcpy(uiStatus.deviceId, "0123456789ABCDEF");
  Adafruit_SSD1351 display;
  MenuSystem menu(display, "unchanged");
  const auto settings = menu.getSettings();
  menu.setBluetoothCallbacks(action, BleUi::renderPage);
  menu.activateMenu(); menu.updateMenu(false, testMillis);
  assert(hasText(display, "Calibrate") && hasText(display, "Bluetooth") && hasText(display, "Exit"));
  for (int i = 0; i < 4; ++i) tap(menu);
  hold(menu); assert(hasText(display, "Pair") && hasText(display, "Status"));
  hold(menu); assert(pairActions == 1 && hasText(display, "No passkey needed"));
  uiStatus.connected = true; uiStatus.mtu = 247;
  menu.updateMenu(false, ++testMillis); assert(hasText(display, "MTU: 247 / 247"));
  uiStatus.mtu = 23;
  menu.updateMenu(false, ++testMillis); assert(hasText(display, "MTU: 23 / 247"));
  hold(menu); assert(cancelActions == 1 && hasText(display, "Forget"));
  tap(menu); tap(menu); hold(menu); assert(hasText(display, "Forget?"));
  hold(menu); assert(forgetActions == 0); // Cancel is initially selected.
  tap(menu); tap(menu); hold(menu); tap(menu); hold(menu);
  assert(forgetActions == 1 && hasText(display, "Bluetooth"));
  hold(menu); assert(cancelActions == 1); // Back from Status does not cancel security.
  tap(menu); tap(menu); tap(menu); hold(menu); // Back to root.
  for (int i = 0; i < 5; ++i) tap(menu);
  hold(menu); assert(!menu.isActive()); // Exit is now the sixth row.
  const auto after = menu.getSettings();
  assert(after.advancedMode == settings.advancedMode && after.lameDisplay == settings.lameDisplay &&
         after.weaponDisplay == settings.weaponDisplay && std::strcmp(after.version, settings.version) == 0);
  menu.activateMenu(); for (int i = 0; i < 3; ++i) tap(menu); hold(menu);
  assert(calibrations == 1); // Calibration remains reachable.
  BleUi::drawIndicator(display); // Bounds also checked in the display fake.
}
int main() {
  pairingPolicy(); identityAndGatt(); menuAndUi();
  std::cout << "BLE Phase 1: discovery/encryption state, identity, GATT, menu and panel bounds passed\n";
}
