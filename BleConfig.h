#ifndef TTARM_BLE_CONFIG_H
#define TTARM_BLE_CONFIG_H

// Standard Bluefruit pairing on both nRF52832 and nRF52840.
// Set to 0 for the radio-off acquisition baseline; no core patch is needed.
#ifndef TTARM_BLE_ENABLED
#define TTARM_BLE_ENABLED 1
#endif
#define TTARM_BLE_BUILD "26-10-31-status"
#define TTARM_BLE_TARGET_MTU 247

#endif
