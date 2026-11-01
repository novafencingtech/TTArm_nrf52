#ifndef MENUSYSTEM_H
#define MENUSYSTEM_H

#include <Adafruit_SSD1351.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

// Ensure C linkage
#ifdef __cplusplus
extern "C" {
#endif
void calibrateSystem();
#ifdef __cplusplus
}
#endif

#define MENU_LONG_PRESS_DURATION 1000  // Milliseconds for a long press
#define MENU_MAX_STR_LEN 24

class MenuSystem {
public:
  // Constructor
  MenuSystem(Adafruit_SSD1351& display);
  MenuSystem(Adafruit_SSD1351& display, const char* version);

  enum displayType {
    GRAPH = 0,
    NUMERIC,
    BAR,
    NONE,
  };

  struct Settings {
    char version[16]="";  // Version string (e.g., "v1.0.0")
    displayType lameDisplay=GRAPH;
    displayType weaponDisplay=GRAPH;
    bool advancedMode=true;
  };

  Settings getSettings();
  // Update menu based on button interaction
  void updateMenu(bool buttonPressed, unsigned long currentTime);

  enum BluetoothAction { PAIR_PHONE, CANCEL_PAIRING, FORGET_PHONES };
  typedef void (*BluetoothActionCallback)(BluetoothAction action);
  typedef void (*BluetoothRenderCallback)(Adafruit_SSD1351&, bool pairingPage, bool force);
  void setBluetoothCallbacks(BluetoothActionCallback action, BluetoothRenderCallback render) {
    bluetoothAction = action;
    bluetoothRender = render;
  }

  const char* displayTypeToString(displayType);

  void loadSettings();
  void saveSettings();
  void resetToDefaults();  // Reset settings to default values

  //Adafruit_LittleFS_Namespace::File *settingsFile;
  //bool isInitialized=false;

  bool isActive() const {
    return menuActive;
  }
  void activateMenu() {
    currentPageIndex = 0;
    initializeMenu();
    menuActive = true;
    menuChanged = true;
    return;
  }
  void printSettings();

  const static uint16_t maxStrLen = 24;

  //displayType lameDispSetting = GRAPH;
  //displayType weaponDispSetting = GRAPH;

private:
  const uint16_t menuColorBLACK = 0x0000;
  const uint16_t menuColorBLUE = 0x001F;
  const uint16_t menuColorPaleBLUE = 0xAEDC;
  const uint16_t menuColorRED = 0xF800;
  const uint16_t menuColorGREEN = 0x07E0;
  const uint16_t menuColorCYAN = 0x07FF;
  const uint16_t menuColorMAGENTA = 0xF81F;
  const uint16_t menuColorORANGE = 0xFD20;
  const uint16_t menuColorYELLOW = 0xFFE0;
  const uint16_t menuColorWHITE = 0xFFFF;
  const uint16_t menuColorGREY = 0xBDF7;
  const uint16_t menuColorDARKBLUE = 0x0007;

  const static uint16_t maxMenuItems = 6;
  uint8_t menuItemsCount = 0;
  Adafruit_SSD1351& display;
  char menuItems[maxMenuItems][maxStrLen];
  char menuHeading[maxStrLen];
  char exitText[maxStrLen];
  Settings settings;
  const char settingsFileName[32]="/settings.bin"; // File name for storing settings  
  //std::map<std::string, std::string> settings; // Store settings in an STL map
  int8_t cursorItemIndex;     // Currently selected menu item
  int8_t currentPageIndex;    // Current page
  int8_t buttonState;         // 0: released, 1: pressed, 2: confirmed
  int8_t highlightItemIndex;  //Which index is currently active
  unsigned long lastButtonPressTime;
  bool menuActive = false;
  bool menuChanged = false;
  bool settingsChanged = false;
  static const int8_t lameSettingsPage = 2;
  static const int8_t weaponSettingsPage = 3;
  static const int8_t advancedModePage = 1;
  static const int8_t calibratePage = 4;
  static const int8_t bluetoothPage = 5;
  static const int8_t bluetoothStatusPage = 6;
  static const int8_t bluetoothPairPage = 7;
  static const int8_t bluetoothForgetPage = 8;
  BluetoothActionCallback bluetoothAction = nullptr;
  BluetoothRenderCallback bluetoothRender = nullptr;


  // Initialize menu with entries
  void initializeMenu();

  // Switch to the next item
  void switchItem();

  // Confirm selection and take action
  void confirmSelection();

  // Render the menu on the screen
  void renderMenu();

  // Draws a white box around a setting
  void drawSelectionBox(int x, int y, int w, int h);

  // Placeholder function for calibration

  void calibrate();

  
};

#endif  // MENUSYSTEM_H
