# BLE Phase 1: basic pairing and status

This version supports **both nRF52832 and nRF52840** using the stock Adafruit
Bluefruit APIs and standard Just Works pairing/bonding. No core patch, passkey,
software ECC backend, architecture guard, or special compiler flag is required.
It replaces the earlier nRF52840-only authenticated-pairing implementation.

Phase 1 sends no application notifications or indications and executes no remote
commands. Public Protocol and Full Identity reads are connection metadata. The
protocol's diagnostic Status notifications remain a Phase 2 feature.

## Using it

1. Build/upload for the physical board: Adafruit Feather nRF52832 or Feather
   nRF52840 Express. Both now run the same BLE manager and GATT code.
2. Scan using an active BLE scanner for `TTArm-XXXXXX`. The exact name and full
   box ID appear under **Settings > Bluetooth > Status**. The name is carried
   in scan response, alongside the tester service UUID in advertising.
3. **Bluetooth > Pair** starts a 60-second fast-discovery window. Select the box
   on the phone and accept the phone's pairing prompt if shown. No code is
   displayed or entered. New pairing is also allowed during normal advertising.
4. The status page shows Connected/Pairing and then **Encrypted** once encryption
   is established. It displays the saved-phone count from the stack's bond files.
   Previously paired phones can reconnect using their saved bond.
5. **Forget** opens a separate confirmation screen. Cancel is selected initially.
   Confirm disconnects the phone and clears saved peripheral bonds/subscriptions.
   For recovery, also forget the tester on the phone before pairing again.

One phone connects at a time. Advertising pauses while connected and restarts
on disconnect. Pair temporarily uses 20 ms advertising for 30 seconds and
152.5 ms for the next 30 seconds; normal advertising uses 546.25 ms. Back ends
fast discovery and cancels an unencrypted connection attempt, while an encrypted
connection continues. An unencrypted connection times out after 60 seconds.
The discovery window is not an authorization/security gate.

The normal-screen dot is green for encrypted, orange for connected/pairing,
cyan for fast discovery, dark blue for ready/disconnected, and gray when BLE is
unavailable or disabled. There is no nRF52832-specific unavailable state.

## Building

The Arduino IDE can build the repository directly: open `TTArm_nrf52.ino`, select
its physical Adafruit board, and use the normal compile/upload commands with the
existing display and FastLED dependencies. Bluefruit is supplied by the Adafruit
nRF52 board package. No modified installed library or custom build configuration
is needed.

An optional CLI helper stages ordinary build inputs without changing the source
or installed libraries:

```powershell
python tools/build_ble.py                        # nRF52832 is the default
python tools/build_ble.py --board feather52840
python tools/build_ble.py --ble-disabled
python tools/test_ble.py
```

The helper accepts `--cli` and `--config` for installation paths and
`--prepare-only` to stage without compiling. It passes no custom compiler flags
or library overrides. Radio-off mode changes only the staged `BleConfig.h` feature
switch. Host tests require a C++17 compiler (`g++` on PATH or `--compiler`).

Firmware output is under `.build/feather52832-standard/output/` or
`.build/feather52840-standard/output/`. Earlier `.build/*-ble/libraries/` copies
are obsolete and are not used. `TTARM_BLE_ENABLED=0` in `BleConfig.h` disables
BLE for an acquisition baseline.

## Implementation and protocol

The [message specification](BLE_MESSAGE_SPECIFICATION.md), revision 2, now uses
**Security Mode 1 Level 2**: encryption without MITM authentication. Identity byte
64 reports 2. Legacy Just Works pairing on nRF52832 and Secure Connections when
provided by the stock stack are both accepted. No measurement payload layout or
UUID changes were made. Capability bits remain zero in this pairing-only phase.

The stock stack owns pairing, key negotiation, bond persistence, and subscription
restoration. The application observes synchronous connection/security events to
update display state. Forget is queued on the stock callback worker after
connection teardown, behind pending stock bond/subscription writes.

Existing `.ino` hooks remain small: initialize before acquisition, service BLE
in the main/menu/button waits, stop before power-off, and restore the normal
screen after menu exit. SAADC priority 6 is used with BLE on **both** MCUs so
radio interrupts can preempt acquisition. Measurement processing, serial
calculation/reset schedules, battery acquisition, settings layout, and mode
numbers remain unchanged. Stock MTU/queue defaults are used; 68-byte Identity
reads use normal Read Blob when necessary. Streaming/throughput tuning comes
later.

## Validation

Verified October 31, 2026 with unmodified Adafruit core 1.3.0 / Bluefruit 0.21.0,
GFX 1.11.7, SSD1351 1.2.8, and FastLED 3.10.0. Both compiled source snapshots
match the working source, with no custom compiler properties or library overrides.

| Check | Result | Flash bytes | Static RAM bytes |
| --- | --- | --- | --- |
| nRF52832, standard S132 build | Pass | 176,452 | 29,604 |
| nRF52840, standard S140 build | Pass | 225,564 | 34,768 |
| Portable state/GATT/menu tests | Pass | — | — |
| Radio-off staging preserves source settings | Pass | — | — |

Sizes are compiler reports, not runtime heap/stack measurements. Existing
CMSIS/Arduino `PI` and FastLED nRF52832 warnings remain. Previous size reports
for the authenticated nRF52840-only implementation do not apply.

Portable tests cover fast-discovery deadlines and uptime wrap, connection
lifecycle/timeouts, observed encryption state, Identity security level/encoding,
GATT UUIDs/properties/permissions/lengths, menu navigation/Forget confirmation,
and panel draw-call bounds. Recording fakes do not test actual radio security
or physical display pixels.

Before hardware qualification, verify on both MCU families and Android/iOS:

- Active scanning finds the name after boot and during Pair; normal advertising
  resumes after the discovery timer or a disconnect.
- Just Works pairing, protected Identity/CCCD access, saved-bond reconnects,
  power-cycle reconnects, rejected/canceled prompts, and local Forget recovery.
- Full 68-byte Identity reads at default MTU, capability bits zero, and no
  application notifications, indications, or executed commands.
- Menu/display restoration, power-off, ADC/capture and GPIO hit/latch timing,
  FastLED, battery checks, serial diagnostics, and watchdog operation with BLE.

No device is flashed automatically by these build helpers.
