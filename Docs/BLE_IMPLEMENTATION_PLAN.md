# Phased BLE implementation plan

October 31, 2026. Planning only; this change does not implement firmware.

The [BLE Message Specification, protocol 2.0 revision 1](BLE_MESSAGE_SPECIFICATION.md) is the wire-format and security authority. The older [initial plan](BLE_Protocol_Initial_Plan.md) is historical; do not implement its protocol 1 messages, validity flags, subscription commands, or UUID placeholders.

## 1. First delivery: pairing and local BLE status

Interpretation of “pairing and status but no messages”: the first delivery implements discovery, locally authorized pairing, bonding/reconnection, and BLE status on the tester's display. It sends **no application notifications or indications**, including the specification's `Status` message. Identity reads are connection setup metadata, included with the GATT foundation. If “status” is intended to mean the protocol's diagnostic heartbeat, that is the next delivery, Phase 2 below.

Standard advertising, GATT discovery/reads, and Bluetooth security exchanges necessarily operate during pairing. There is no measurement streaming, command execution, custom phone handshake, or BLE debug-text channel in Phase 1. A phone BLE test client is sufficient initially; a production phone application is separate work.

Keep existing measurement modes, button meanings, serial output, calibration, and settings behavior. Put BLE logic in new `.h/.cpp` files, with small integration hooks in the existing `.ino` files. This document records the intended edits; no `.ino` edits are needed to deliver the plan.

## 2. Findings in the current sketch

| Existing location | Finding and implementation consequence |
| --- | --- |
| `Feather_TestBox_v0.ino`: `setup()` / `loop()` | No Bluefruit initialization exists. Add one initialization hook and a short, bounded service hook near the start of `loop()`, before paths that can return early. Initialize the BLE stack before starting acquisition interrupts, after the power latch, filesystem, and display are ready. Verify the final order in Phase 0. |
| `ModesStates.ino`: `menuLoop()` | The menu owns a blocking loop. Call the same BLE foreground service there so passkeys, deadlines, and status continue updating. |
| `ModesStates.ino`: `checkButtonState()` | The held-button loop also blocks foreground work. Service BLE there without changing the existing power/settings thresholds. Audit calibration and other long operations for the same issue. |
| `MenuSystem.h/.cpp` | Root menu currently has four items and capacity for six. Exit is selected by `case calibratePage` and drawn at fixed row `5 * 22`. Adding an item requires fixing both assumptions. |
| `oledDisplaySettings.h` | Display is 128 by 128. A fifth item at the existing spacing would collide with Exit. Adjust the menu layout locally and verify all pages. |
| `Init_ADC.ino`, main sketch ISR, FastLED use | Direct SAADC/NVIC access, interrupt-driven acquisition, and WS2812 updates need coexistence testing with the active SoftDevice. Do not assume a BLE example will integrate unchanged. |
| `DisplayHandling.ino`: `writeSerialOutput()` | Diagnostic rate/count are calculated about every 5 seconds; `numSamples` is then reset. Phase 2 must cache this existing result rather than calculate/reset a second counter. |
| `ModesStates.ino`: `CheckBatteryStatus()` | Existing voltage acquisition runs at startup and about every 10 seconds. Phase 2 reuses it without adding BLE-driven ADC activity. |
| `MenuSystem::Settings` | Persisted as raw structure bytes. Pairing is an action, not a new saved setting; keep bond storage separate and preserve the settings file layout. |

## 3. Phase 0: confirm the board/core integration

Deliver a recorded build configuration and a short hardware feasibility result before implementing the full UI.

1. Identify and pin the actual Adafruit nRF52 core, SoftDevice, board target, library versions, and build command. The source supports nRF52832 and nRF52840 selections; qualify each target that will be shipped. No local `arduino-cli` or core installation was found in the usual locations during this planning review, so compilation and hardware compatibility remain unverified.
2. Establish a baseline build and run of the existing measurement modes, capture, serial diagnostics, menu, calibration, battery acquisition, and power-off behavior.
3. Prove **LE Security Mode 1 Level 4** on the chosen target: authenticated LE Secure Connections, 128-bit encryption key, display-only passkey entry. Check actual negotiated security and bond restoration, not just a successful connect callback.
4. Check the selected core's way to reject new pairing outside a local window, detect secure reconnection, reject downgrade, limit/store bonds, and delete bonds locally. A foreground disconnect after an unauthorized bond was saved is too late; gate the security procedure before acceptance/persistence. If a core extension is necessary, isolate and document it rather than spreading SDK calls through the sketch.
5. Audit interrupt priorities and stack-owned resources. Verify permitted SAADC/GPIO interrupt configuration and SoftDevice-safe critical sections; test measurement timing and FastLED behavior while advertising, pairing, saving a bond, and reconnecting. Change only demonstrated incompatibilities.
6. Record RAM/flash use and watchdog behavior with one peripheral connection and the full GATT database. Avoid throughput tuning until basic integration passes.

**Specific security risk:** upstream Bluefruit security source conditions its LESC cryptography on `NRF_CRYPTOCELL`. Its `setPIN()` path disables LESC; it is unsuitable for this specification. Verify the pinned core and both board targets rather than assuming equal support. Use the passkey display callback with the required secure association. If a target cannot meet Level 4, resolve its stack support or supported-target policy before shipping; do not silently weaken security. See the [upstream security implementation](https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/libraries/Bluefruit52Lib/src/BLESecurity.cpp) and [security interface](https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/libraries/Bluefruit52Lib/src/BLESecurity.h), reviewed for this plan; pin a commit/version during implementation.

## 4. Phase 1: discovery, pairing, bonds, and display status

### 4.1 Small module boundary

Proposed new files:

| Module | Responsibility |
| --- | --- |
| `BleManager.h/.cpp` | Stack lifecycle, advertising, one connection, pairing authorization/deadlines, security checks, bonds, and a read-only status snapshot. No dependency on acquisition globals or display objects. |
| `BleGatt.h/.cpp` | Specification UUIDs, complete stable GATT registration, access permissions, and explicit Public Protocol/Full Identity encoders. No live-message codecs yet. |
| `BleUi.h/.cpp` | Render the Bluetooth page, pairing passkey, and a compact local status indicator using an explicitly passed display reference. |

Keep the manager interface small, for example `begin()`, `poll(now)`, `startPairing(now)`, `cancelPairing()`, `getStatus()`, and a locally confirmed `forgetBonds()`. These are proposed interfaces, not existing functions. Keep Bluefruit types inside the implementation where practical.

BLE callbacks copy bounded event data into synchronized state/mailboxes; foreground code updates the display. Do not render, wait for buttons, run ADC/calibration, or execute acquisition-mode changes from callbacks. Handle security decisions in the stack's required context. Tag deferred work with connection generation and discard it after disconnect/reconnect; do not reuse an old passkey or status event for a new phone.

### 4.2 Advertising and GATT foundation

- Use the exact tester service UUID `4a1e0000-3bb5-44b4-b566-8d22e72da6f5`. Advertise legacy connectable LE1M service discovery and put `TTArm-xxxxxx` in scan response, using the device ID's low 24 bits. Show the full ID locally for identification.
- Configure one peripheral connection and no central role. Stop connectable advertising while connected. Normal boot allows saved-bond reconnection; it does not authorize a new bond. With no bonds and no pairing window, remain locally available but reject new enrollment.
- During the 60-second local pairing window, use the specification's recommended 20 ms interval for the first 30 seconds and 152.5 ms thereafter. Outside it, use 546.25 ms for known-device discovery. Pairing authorization is enforced by security policy, independently of advertising rate.
- Register every specified characteristic, including reserved Capture, with the specified properties, variable maximum lengths, individual CCCDs, and security permissions from the outset. Keep handles stable where possible and apply the stack's Service Changed/bonded-client rules for database changes.
- Serve Public Protocol as exactly `54 54 42 4C 02 00 01 00`. Serve the 68-byte Full Identity only at Level 4; support normal long reads at MTU 23. Use a stable nonzero device ID, fresh nonzero random boot ID, and explicit byte encoding. Freeze Identity throughout each connection.
- Set capabilities only for implemented, usable features. Phase 1 has no stream or command capabilities; leave battery capability clear until complete usable BLE diagnostics are integrated. Do not equate local measurement support with implemented BLE streams.
- Configure stack-managed GAP/GATT. Prepare the specified MTU ceiling and connection requests if resources permit, but validate operation at MTU 23. Negotiated values are observations, not assumptions.
- Even if CCCDs are enabled or restored from a bond, Phase 1 publishes nothing. Reject Control writes at ATT without executing them. This is a development milestone, not complete protocol 2.0 conformance; command/result behavior is delivered later.

### 4.3 One new root menu item

Add **Bluetooth** as the fifth settings item. Its submenu contains **Pair**, **Status**, **Forget**, and **Back**. Pair starts the authorized pairing window; Forget enters an explicit on-device confirmation page. This provides recovery without another root item or a changed power-button gesture.

Use the menu's existing short-press navigation and long-press confirmation. Pairing is an asynchronous page/state, not a blocking 60-second function. Selecting Pair while a phone is connected reports that the box is busy; do not silently disconnect it or admit a second phone. Reopening Pair when disconnected starts a fresh window and invalidates any earlier attempt/code.

Concrete menu fixes:

1. Set root count to five and make Exit selection depend on `menuItemsCount`, rather than the calibration page number. Check Exit before dereferencing a selected menu item.
2. Give Bluetooth a distinct page/action identifier. Use a small action callback registered from setup to request BLE operations without coupling `MenuSystem` directly to Bluefruit or acquisition code.
3. Fit the root menu into 128 pixels: for example item rows at y=20, 38, 56, 74, 92 and Exit at y=110. Preserve adequate highlight spacing and use shorter labels or smaller text where needed. Keep submenu Exit/Back positioned from its layout, not an assumed fifth row.
4. Service BLE and dynamic page updates inside `menuLoop()`. Repaint passkey/countdown only when needed. Give the Bluetooth page ownership of its display region so menu rendering does not immediately erase the passkey.
5. Back from the pairing page cancels enrollment and clears the code. Restore/invalidate the normal display layout on leaving the menu so cached display state cannot leave pairing text behind.

### 4.4 State and security rules

Keep connection/security state separate from pairing-window authorization and stored-bond presence. A saved bond, an open window, a link, and a secure session are different facts.

| Event/state | Local behavior |
| --- | --- |
| Boot / disconnected | Show `BLE ready` or `BLE reconnect`; no active passkey. Saved phones may restore security. |
| Local Pair action | Open a 60-second authorization window; show box name/full ID and remaining time. |
| New phone connects inside window | Show `BLE securing`. Display the stack-generated fresh six-digit passkey when requested, preserving leading zeros. The passkey is not generated/displayed before the pairing attempt exists. |
| Level 4 established and bond saved | Show `BLE secure`, close enrollment, and clear passkey. Do not display secure merely because pairing started or a bond exists. If persistence fails, report it explicitly. |
| Saved phone reconnects | Restore authenticated encryption without another passkey; show secure only after current-link verification. |
| Pairing canceled, failed, or expired | Clear passkey and authorization; disconnect an untrusted pending peer and return to reconnect advertising. Timeout also applies to an in-progress enrollment, so a stalled phone cannot keep it open. |
| Disconnect | Clear connection-specific state immediately; restart appropriate advertising. After a completed bond, new pairing still requires another local action. |
| Weaker security / new pairing outside window | Reject; no bond replacement and no protected access. Public discovery/version reads may remain available. Bound untrusted connection time so it cannot indefinitely occupy the only slot. |
| Bond capacity full | Show `Bonds full`; require local removal. Never silently evict a phone. |
| Confirmed Forget | Close enrollment, disconnect any active phone, clear saved phone bonds/associated subscription state, then return to ready. Do not erase application settings, calibration, or device identity. Explain locally that the phone may also need to forget its saved bond. |

Choose an explicit supported bond capacity during Phase 0; four saved phones is the historical plan's starting proposal, not a requirement of the message specification. Start recovery with Forget All; selective removal can follow if needed. Rate-limit failed attempts, use wrap-safe deadline arithmetic, and keep passkeys/keys out of serial diagnostics.

Show local BLE state on the Bluetooth page and a small indicator on normal measurement screens. Treat it as a separate UI concern from the protocol `Status` payload. Update on state changes and restore it after full-screen redraws; avoid overwriting readings, battery, or existing mode labels. Local Pair counts as button activity; background advertising/reconnects do not continually reset the existing idle/power-off timer. Pairing is canceled by actual shutdown, and BLE work must not delay power-off.

### 4.5 Expected existing-file changes for Phase 1

| File | Small intended edit |
| --- | --- |
| `Feather_TestBox_v0.ino` | Include new interfaces, register menu action callback, initialize BLE at the audited startup point, and call foreground BLE/UI service near the beginning of `loop()`. |
| `MenuSystem.h/.cpp` | Fifth root item, Bluetooth pages/action callback, dynamic Exit handling, and compact layout. Preserve persisted `Settings` layout. |
| `ModesStates.ino` | Service hook in `menuLoop()` and held-button loop; bounded cancel/stop hook before power-off. Add hooks in other long operations only where the audit proves necessary. |
| `DisplayHandling.ino` | Small normal-screen BLE indicator integration and display invalidation if required; keep rendering implementation in `BleUi`. |
| `Init_ADC.ino` | No planned functional change. Any necessary interrupt fix must be justified by Phase 0 evidence and separately reviewed. |
| `eeprom.ino`, acquisition classes | No planned Phase 1 changes. |

Use a compile-time BLE enable switch for baseline comparison. No new acquisition mode, renumbered mode enum, wholesale loop rewrite, or BLE logic inside the ADC ISR is needed for this phase.

### 4.6 Phase 1 acceptance gate

Demonstrate on each supported board and both Android/iOS:

- Correct service advertising/name, one connected phone, and repeatable local menu entry/exit with all existing settings reachable. Inspect every menu screen for clipping and Exit overlap.
- Fresh passkey pairing only after local Pair; wrong code, cancellation, expiry during pairing, and attempted enrollment outside the window all fail safely. A second phone cannot replace the connected/bonded peer silently.
- Measured Level 4 security and 128-bit key; weaker/legacy attempts rejected. Public Protocol remains readable; Full Identity and CCCD writes are protected. Full Identity long read works at MTU 23.
- Secure reconnection without code after link loss and power cycle; stale phone bonds, capacity-full behavior, confirmed local recovery, and canceled Forget handled visibly. Settings/calibration remain intact.
- Display transitions remain responsive in the normal loop, menu, held-button loop, and audited long operations. Disconnect removes the secure indication immediately. Old callbacks do not restore old status/passkeys.
- GATT trace shows **zero application notifications/indications and zero executed commands**, including after restored CCCDs. Identity read values match the specification.
- Existing acquisition modes, capture behavior, serial diagnostic cadence, battery checks, watchdog, and manual/idle power-off pass baseline comparison with BLE enabled. Measure rather than assume timing compatibility.

Phase 1 is complete only when these checks pass; a compiling sketch or a phone showing “connected” is insufficient.

## 5. Phase 2: diagnostic Status messages only

This is the first delivery of application messages. Keep all measurement streams and remote controls disabled.

1. Add `BleProtocol.h/.cpp` with explicit encoders for the 14-byte header and 6-byte notification wrapper. No packed-struct serialization, JSON, or ASCII debug messages.
2. Add a small cache hook at the existing serial heartbeat calculation, before its existing reset, to retain the same interval count and rounded Hz. Cache valid measured battery millivolts from the existing battery update. BLE must not reset `numSamples`, alter the serial schedule, or trigger extra battery acquisition. Handle coherency where the counter is ISR-owned using the audited stack-safe approach.
3. Track actual mode changes at the common mode owner, including any direct setters found in the audit. Increment `mode_epoch` once per actual transition; repeated selection of the same mode does not increment it. Use an explicit mapping from local modes to protocol values rather than depending on enum layout. Start tracking at boot, not first subscription.
4. Send complete initialized Status on subscription/mode change and at 1 Hz, only with the current Level 4 connection and enabled Status CCCD. Respect restored CCCDs. Wait for count, rate, and voltage initialization; do not send placeholders. Status is a 10-byte payload, 24-byte logical message, and 30-byte one-fragment value. At MTU 23 it requires two notifications with 14- and 10-byte logical slices, giving value lengths 20 and 16.
5. Keep a bounded nonblocking sender, per-connection Status sequence/transport IDs, and fragment boundaries fixed for an active transmission. Reset connection-local state on disconnect; preserve boot-local epoch. No Status transmission after power-off; any pre-shutdown attempt is best effort and bounded.
6. Set the battery capability when usable diagnostics are available, and keep Identity immutable during each connection. Verify initialization waiting, exact bytes, security/subscriptions, 1 Hz behavior, MTU 23 and larger MTUs, backpressure, reconnect, mode changes, and counter wrap. Confirm serial diagnostic results/cadence are unchanged.

## 6. Later deliveries

| Phase | Scope | Gate |
| --- | --- | --- |
| 3a | Combined Cable stream at 30 Hz, then complete Weapon and Probe streams; common snapshot/queue machinery | Exact protocol vectors, coherent initialized fields, milliohm/OPEN conversion, epoch handling, default-MTU fragmentation, bounded backpressure, measured delivery and acquisition coexistence. Enable each capability only when its full stream works. |
| 3b | Lights accepted-state changes plus 1 Hz full-state refresh | Preserve hardware input/latch semantics and bounded FIFO behavior; verify short pulses, quiet-state refresh, overload, mode changes, and reconnection. |
| 4 | Control/Command Result: SetBoxMode, registered settings, manual calibration | Enforce security and enabled Result indications before accepting Control. Direct compact values have no live header/wrapper. Use existing local mode/settings/calibration owners and specification result semantics, including UNSUPPORTED for unavailable operations. |
| 5 | Full phone integration and release qualification | Identity/session isolation, multiple nearby boxes, independent phone sessions, cross-platform reconnects, security, freshness, overload, and all specification conformance checks. |

Capture waveform transfer and retained event history remain outside protocol 2.0. Do not add them as an implied continuation of local capture support.

Recommended implementation order: Phase 0 evidence, Phase 1 transport/security, Phase 1 menu/display and recovery, Phase 1 acceptance, Phase 2 Status, then individual measurement and control deliveries. Keep each change reviewable and add existing-sketch hooks only when its phase requires them.
