#include "BleUi.h"
#include "BleManager.h"
#include <string.h>
#include <stdio.h>

void BleUi::renderPage(Adafruit_SSD1351& display, bool pairingPage, bool force) {
  const auto status = BleManager::getStatus(millis());
  static BleManager::Status previous = {};
  if (!force && previous.message == status.message && previous.connected == status.connected &&
      previous.encrypted == status.encrypted && previous.secondsLeft == status.secondsLeft &&
      previous.bonds == status.bonds && previous.generation == status.generation && previous.mtu == status.mtu) return;
  previous = status;
  display.fillScreen(0);
  display.setFont(nullptr);
  display.setTextWrap(false);
  display.setTextSize(2);
  display.setTextColor(0xffff, 0);
  display.setCursor(0, 0); display.print(pairingPage ? "Pair phone" : "Bluetooth");
  display.setTextSize(1);
  display.setCursor(0, 23); display.print(status.message);
  display.setCursor(0, 36); display.print(status.name);
  display.setCursor(0, 47); display.print(status.deviceId);
  display.setCursor(0, 66); display.print("No passkey needed");
  char mtuText[22];
  if (status.connected) snprintf(mtuText, sizeof(mtuText), "MTU: %u / %u", unsigned(status.mtu), unsigned(TTARM_BLE_TARGET_MTU));
  else snprintf(mtuText, sizeof(mtuText), "MTU: -- / %u", unsigned(TTARM_BLE_TARGET_MTU));
  display.setCursor(0, 78); display.print(mtuText);
  display.setTextSize(1);
  display.setTextColor(0xffff, 0);
  display.setCursor(0, 91);
  if (status.pairing) {
    display.print("Discover: "); display.print(status.secondsLeft); display.print("s left");
  } else {
    display.print("Saved phones: "); display.print(status.bonds);
  }
  display.setCursor(0, 105);
  display.print(status.encrypted ? "Link encrypted" : status.connected ? "Link connected" : "Link disconnected");
  display.setCursor(0, 119); display.print("Hold: Back");
  display.setTextWrap(true);
}

void BleUi::drawIndicator(Adafruit_SSD1351& display) {
  const auto status = BleManager::getStatus(millis());
  const uint16_t color = !status.available ? 0x4208 : status.encrypted ? 0x07e0 :
                         status.connected ? 0xfd20 : status.pairing ? 0x07ff : 0x0010;
  // Below the battery (y=0..9), outside title text (x=0..114) and readings.
  display.fillRect(121, 11, 7, 7, 0);
  display.fillCircle(124, 14, 2, color);
}
