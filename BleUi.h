#ifndef TTARM_BLE_UI_H
#define TTARM_BLE_UI_H
#include <Adafruit_SSD1351.h>
namespace BleUi {
void renderPage(Adafruit_SSD1351& display, bool pairingPage, bool force);
void drawIndicator(Adafruit_SSD1351& display);
}
#endif
