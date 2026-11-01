#include "MenuSystem.h"

// Uncomment and adjust the following line if using LittleFS
// Adafruit_LittleFS_Namespace::File settingsFile(Adafruit_LittleFS);

// Constructor
MenuSystem::MenuSystem(Adafruit_SSD1351& display)
  : display(display) {
  cursorItemIndex = 0;
  currentPageIndex = 0;
  buttonState = 0;
  lastButtonPressTime = 0;

  // Default settings in the map
  menuActive = false;
  initializeMenu();
}

MenuSystem::MenuSystem(Adafruit_SSD1351& display, const char* version)
  : MenuSystem(display) {

  // Issue 1 Fix: Ensure strncpy null-terminates the string
  strncpy(settings.version, version, sizeof(settings.version) - 1);
  settings.version[sizeof(settings.version) - 1] = '\0';

  //resetToDefaults();
  //loadSettings();
}

MenuSystem::Settings MenuSystem::getSettings() {
  return settings;
}

void MenuSystem::resetToDefaults() {
  // Set default values for the settings
  //strncpy(settings.version, CURRENT_VERSION, sizeof(settings.version));
  //strncpy(settings.version, "Default", sizeof(settings.version));
  Serial.println("Loading default settings...");
  settings.lameDisplay = GRAPH;
  settings.weaponDisplay = GRAPH;
  settings.advancedMode = false;
  display.fillScreen(menuColorBLACK);
  display.setCursor(0, 50);
  display.setTextColor(menuColorORANGE);
  display.println("Defaults\n  loaded");
  delay(1000);
}

const char* MenuSystem::displayTypeToString(displayType val) {
  switch (val) {
    case GRAPH:
      return "Graph";
    case NUMERIC:
      return "Numeric";
    case BAR:
      return "Bar";
    case NONE:
      return "None";
    default:
      return "Error";
  }
}

// Update menu based on button interaction
void MenuSystem::updateMenu(bool buttonPressed, unsigned long currentTime) {
  // Issue 2 Fix: Removed unused debounceTime
  // unsigned long debounceTime = 20;

  if (buttonPressed) {
    if (buttonState == 0) {
      lastButtonPressTime = currentTime;
      buttonState = 1;  // Button is pressed
    }
    if ((buttonState == 1) && (currentTime - lastButtonPressTime >= MENU_LONG_PRESS_DURATION)) {
      // Long press detected, confirm the selected item
      buttonState = 2;
      confirmSelection();
    }
  } else {
    switch (buttonState) {
      case 0:  // Button not pressed
        break;
      case 1:  // Button was pressed and is now released
        switchItem();
        break;  // Issue 2 Fix: Added missing break statement
      case 2:   // Confirmed has released
        break;
      default:
        break;
    }
    buttonState = 0;
  }
  renderMenu();
}

// Initialize menu with entries
void MenuSystem::initializeMenu() {
  //printSettings();
  //Serial.print("Lame Display: ");
  //Serial.println(displayTypeToString(settings.lameDisplay));
  if (currentPageIndex == 0) {
    strcpy(menuHeading, "Settings");
    strcpy(menuItems[calibratePage - 1], "Calibrate");
    strcpy(menuItems[lameSettingsPage - 1], "Lame");
    strcpy(menuItems[weaponSettingsPage - 1], "Weapon");
    strcpy(menuItems[advancedModePage - 1], "User Mode");
    strcpy(menuItems[bluetoothPage - 1], "Bluetooth");
    strcpy(exitText, "Exit");
    menuItemsCount = 5;
    cursorItemIndex = 0;
    highlightItemIndex = menuItemsCount + 2;
  } else if (currentPageIndex == lameSettingsPage) {
    strcpy(menuHeading, "Lame");
    strcpy(menuItems[0], "Graph");
    strcpy(menuItems[1], "Numeric");
    strcpy(exitText, "Back");
    switch (settings.lameDisplay) {
      case GRAPH:
        highlightItemIndex = 0;
        break;
      case NUMERIC:
        highlightItemIndex = 1;
        break;
    }
    cursorItemIndex = 0;
    menuItemsCount = 2;
  } else if (currentPageIndex == weaponSettingsPage) {
    strcpy(menuHeading, "Weapon");
    strcpy(menuItems[0], "Graph");
    strcpy(menuItems[1], "Numeric");
    strcpy(exitText, "Back");
    switch (settings.weaponDisplay) {
      case GRAPH:
        highlightItemIndex = 0;
        break;
      case NUMERIC:
        highlightItemIndex = 1;
        break;
    }
    cursorItemIndex = 0;
    menuItemsCount = 2;
  } else if (currentPageIndex == advancedModePage) {
    strcpy(menuHeading, "Box Mode");
    strcpy(menuItems[0], "Basic");
    strcpy(menuItems[1], "Advanced");
    strcpy(exitText, "Back");
    if (settings.advancedMode) {
      highlightItemIndex = 1;
    } else {
      highlightItemIndex = 0;
    }
    cursorItemIndex = 0;
    menuItemsCount = 2;
  } else if (currentPageIndex == bluetoothPage) {
    strcpy(menuHeading, "Bluetooth");
    strcpy(menuItems[0], "Pair");
    strcpy(menuItems[1], "Status");
    strcpy(menuItems[2], "Forget");
    strcpy(exitText, "Back");
    menuItemsCount = 3;
    cursorItemIndex = 0;
    highlightItemIndex = -1;
  } else if (currentPageIndex == bluetoothForgetPage) {
    strcpy(menuHeading, "Forget?");
    strcpy(menuItems[0], "Cancel");
    strcpy(menuItems[1], "Confirm");
    strcpy(exitText, "Back");
    menuItemsCount = 2;
    cursorItemIndex = 0; // Never preselect destructive confirmation.
    highlightItemIndex = -1;
  } else if (currentPageIndex == bluetoothPairPage || currentPageIndex == bluetoothStatusPage) {
    menuItemsCount = 0;
    cursorItemIndex = 0;
    highlightItemIndex = -1;
  }
}

// Switch to the next item
void MenuSystem::switchItem() {
  cursorItemIndex++;
  if (cursorItemIndex >= menuItemsCount + 1) {
    cursorItemIndex = 0;
  }
  menuChanged = true;
}

// Confirm selection and take action
void MenuSystem::confirmSelection() {
  if (currentPageIndex == bluetoothPairPage || currentPageIndex == bluetoothStatusPage) {
    if (currentPageIndex == bluetoothPairPage && bluetoothAction) bluetoothAction(CANCEL_PAIRING);
    currentPageIndex = bluetoothPage;
  } else if (cursorItemIndex == menuItemsCount) {
    // Exit/Back is a separate row, never an element of menuItems.
    if (currentPageIndex == 0) {
      menuActive = false;
      if (settingsChanged) saveSettings();
    } else if (currentPageIndex == bluetoothForgetPage) currentPageIndex = bluetoothPage;
    else currentPageIndex = 0;
  } else if (currentPageIndex == 0) {
    if (cursorItemIndex == calibratePage - 1) calibrate();
    else currentPageIndex = cursorItemIndex + 1;
  } else if (currentPageIndex == bluetoothPage) {
    if (cursorItemIndex == 0) {
      if (bluetoothAction) bluetoothAction(PAIR_PHONE);
      currentPageIndex = bluetoothPairPage;
    } else if (cursorItemIndex == 1) currentPageIndex = bluetoothStatusPage;
    else currentPageIndex = bluetoothForgetPage;
  } else if (currentPageIndex == bluetoothForgetPage) {
    if (cursorItemIndex == 1 && bluetoothAction) bluetoothAction(FORGET_PHONES);
    currentPageIndex = cursorItemIndex == 1 ? bluetoothStatusPage : bluetoothPage;
  } else {
    if (currentPageIndex == lameSettingsPage) settings.lameDisplay = cursorItemIndex == 0 ? GRAPH : NUMERIC;
    if (currentPageIndex == weaponSettingsPage) settings.weaponDisplay = cursorItemIndex == 0 ? GRAPH : NUMERIC;
    if (currentPageIndex == advancedModePage) settings.advancedMode = cursorItemIndex == 1;
    settingsChanged = true;
  }
  menuChanged = true;
  initializeMenu();
}

// Render the menu on the screen.
void MenuSystem::renderMenu() {
  if (menuActive && (currentPageIndex == bluetoothPairPage || currentPageIndex == bluetoothStatusPage)) {
    if (bluetoothRender) bluetoothRender(display, currentPageIndex == bluetoothPairPage, menuChanged);
    menuChanged = false;
    return;
  }
  if (!menuChanged) return;
  menuChanged = false;
  display.fillScreen(menuColorBLACK);
  if (!menuActive) return;
  display.setFont(nullptr);
  display.setTextWrap(false);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.setTextColor(menuColorWHITE);
  display.print(menuHeading);
  // Five root items plus Exit fit below the heading on the 128px panel.
  const int rowHeight = 18;
  const int firstRow = 20;
  for (int i = 0; i < menuItemsCount; ++i) {
    const int y = firstRow + i * rowHeight;
    display.setTextColor(i == cursorItemIndex ? menuColorPaleBLUE : menuColorGREY);
    if (i == cursorItemIndex) { display.setCursor(0, y); display.print('>'); }
    display.setCursor(15, y); display.print(menuItems[i]);
    if (i == highlightItemIndex)
      display.drawRect(13, y - 1, 12 * strlen(menuItems[i]) + 4, 18, menuColorCYAN);
  }
  const int exitY = firstRow + menuItemsCount * rowHeight;
  display.setTextColor(menuColorRED);
  if (cursorItemIndex == menuItemsCount) {
    display.setTextColor(menuColorPaleBLUE);
    display.setCursor(56, exitY); display.print('>');
  }
  display.setCursor(70, exitY); display.print(exitText);
  if (currentPageIndex == bluetoothForgetPage) {
    display.setTextSize(1);
    display.setCursor(0, 91); display.print("Remove ALL phones");
    display.setCursor(0, 104); display.print("Also forget on phone");
  }
  display.setTextWrap(true);
}

// Draw a white box around a setting
void MenuSystem::drawSelectionBox(int x, int y, int w, int h) {
  // Draw a box around the setting
}

// Placeholder function for calibration
void MenuSystem::calibrate() {  
  calibrateSystem();  
}

void MenuSystem::loadSettings() {
  // Issue 2 Fix: Initialize InternalFS before opening files

  Adafruit_LittleFS_Namespace::File settingsFile = InternalFS.open(settingsFileName, Adafruit_LittleFS_Namespace::FILE_O_READ);
  //settingsFile=InternalFS.open(settingsFileName, Adafruit_LittleFS_Namespace::FILE_O_READ);

  if (settingsFile) {  // Equivalent to checking if the file was successfully opened
    Settings readSettings;
    if (settingsFile.read(reinterpret_cast<uint8_t*>(&readSettings), sizeof(Settings)) == sizeof(Settings)) {
      // Check the version before applying the settings
      if (strncmp(readSettings.version, settings.version, sizeof(settings.version)) == 0) {
        settings = readSettings;  // Only apply settings if the version matches
        /*display.fillScreen(menuColorBLACK);
        display.setTextColor(menuColorCYAN);
        display.setCursor(0, 50);
        display.println("Settings\n  loaded");
        delay(1000);*/
      } else {
        resetToDefaults();
      }
    } else {
      Serial.println("Failed to read settings from file");
      resetToDefaults();
    }
    settingsFile.close();
    printSettings();
  } else {
    Serial.println("Settings file not found, loading defaults");
    // If the file doesn't exist or fails to read, use default settings
    resetToDefaults();
  }
}

void MenuSystem::saveSettings() {
  // Issue 2 Fix: Initialize InternalFS before opening files
  if (!InternalFS.begin()) {
    Serial.println("Failed to initialize filesystem");
    return;
  }

  if (InternalFS.exists(settingsFileName)) {
    if (InternalFS.remove(settingsFileName)) {
      Serial.println("Settings file deleted");
    }
  }

  Adafruit_LittleFS_Namespace::File settingsFile = InternalFS.open(settingsFileName, Adafruit_LittleFS_Namespace::FILE_O_WRITE);
  //settingsFile=InternalFS.open(settingsFileName, Adafruit_LittleFS_Namespace::FILE_O_WRITE);

  if (settingsFile) {  // Equivalent to checking if the file was successfully opened
    // Ensure the version is set correctly before saving
    //strncpy(settings.version, CURRENT_VERSION, sizeof(settings.version));
    if (settingsFile.write(reinterpret_cast<const uint8_t*>(&settings), sizeof(Settings)) == sizeof(Settings)) {
      display.fillScreen(menuColorBLACK);
      display.setTextColor(menuColorCYAN);
      display.setTextSize(2);
      display.setCursor(0, 50);
      display.println("Settings\n  saved");
      delay(1000);
    } else {
      Serial.println("Failed to write settings to file");
    }
    settingsFile.close();
    if (settingsFile.size()<=sizeof(Settings)) {
      Serial.println("Error:  Settings file is incorrect or zero size");
    }
    settingsChanged = false;
    //printSettings();
  } else {
    Serial.println("Failed to open settings file for writing");
  }
}


void MenuSystem::printSettings() {
    Serial.println("Settings Status:");
    Serial.print("Version: ");
    Serial.println(settings.version);
    
    Serial.print("Lame Display: ");
    Serial.println(displayTypeToString(settings.lameDisplay));
    
    Serial.print("Weapon Display: ");
    Serial.println(displayTypeToString(settings.weaponDisplay));
    
    Serial.print("Advanced Mode: ");
    Serial.println(settings.advancedMode ? "Enabled" : "Disabled");
    
    Serial.println(); // For spacing between different outputs
}

