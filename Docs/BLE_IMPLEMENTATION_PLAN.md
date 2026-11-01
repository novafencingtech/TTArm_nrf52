# Phased BLE implementation plan

Updated October 31, 2026 for the user's compatibility-first requirement. The
majority of testers use **nRF52832**, and the implementation must support both
nRF52832 and nRF52840. Standard/minimal security is sufficient. Use stock
Bluefruit APIs and ordinary Arduino builds; avoid custom cryptography backends,
core patches, architecture-specific BLE gates, and special compiler settings.

The [message specification](BLE_MESSAGE_SPECIFICATION.md), revision 2, governs
wire format and the revised encrypted Just Works security contract. The older
[initial plan](BLE_Protocol_Initial_Plan.md) remains historical. Implementation
and build/test details are in [BLE_PHASE1.md](BLE_PHASE1.md).

## Phase 1: pairing and local status, no application messages

- Advertise the specified service as `TTArm-XXXXXX` on both MCUs; one connected
  phone at a time. Keep normal discovery/reconnection available after boot.
- Add one Bluetooth settings item with Pair, Status, Forget, and Back. Pair
  starts 60 seconds of fast discovery. Use standard Just Works pairing, with
  no passkey. Pairing is allowed whenever advertising; the discovery timer is
  not a security authorization gate.
- Use stock pairing, bond storage, and encrypted reconnects. Full Identity and
  CCCD writes require Mode 1 Level 2 encryption. Reflect the actual encrypted
  link state in the UI. Keep recovery local with confirmed Forget All.
- Register the complete stable GATT database and encode Public Protocol/Full
  Identity explicitly. Identity reports required security level 2 and zero
  stream/command/diagnostic capability bits. Deny Control writes at ATT and send
  no application notifications or indications, including diagnostic Status.
- Keep BLE implementation in new modules. Limit existing sketch edits to setup,
  loop/wait servicing, menu/display integration, and shutdown. Preserve settings
  layout, acquisition modes, calibration, and serial/battery schedules. Set a
  SoftDevice-compatible SAADC interrupt priority on both boards.
- Build with the unmodified installed core. Keep a simple radio-off feature
  switch for comparison. Use stock MTU/queue defaults for this phase.

Acceptance: compile both boards and radio-off variants, run portable state/GATT/
menu tests, and qualify scanning, pairing, reconnects, recovery, display behavior,
and acquisition timing on physical nRF52832/nRF52840 testers and Android/iOS.
Compilation alone is not hardware qualification.

## Phase 2: diagnostic Status only

- Add explicit encoders for the 14-byte header and 6-byte notification wrapper.
- Cache the existing serial diagnostic count/rate and battery measurement; do not
  recalculate/reset counts or trigger acquisition for BLE. Wait until initialized.
- Track real mode transitions/epochs at the common mode owner.
- Publish the complete specified Status at 1 Hz and on subscription/mode change,
  only on an encrypted link with its CCCD enabled. Support default MTU
  fragmentation and bounded nonblocking backpressure.
- Verify exact bytes, initialization waiting, subscriptions/restoration, mode
  changes, reconnects, and unchanged serial/battery behavior.

## Phase 3: measurement streams

Add combined Cable, then complete Weapon and Probe streams at 30 Hz; add Lights
changes and 1 Hz state refresh. Enable each capability only when all its fields
work. Test coherent snapshots, signed milliohms/OPEN conversion, mode epochs,
fragmentation, bounded queues, actual delivery, and acquisition coexistence.
Tune throughput using measured requirements after the basic implementation works.

## Phase 4: remote controls and release qualification

Add compact Control/Command Result operations through existing local owners.
Require encryption and enabled Result indications before accepting Control.
Implement the specified unsupported/settings/calibration results and local-mode
precedence. Qualify phone/device identity isolation, multiple nearby testers,
reconnects, freshness, overload, and the full conformance checklist.

Capture waveform transfer and retained event history remain outside protocol 2.0.
