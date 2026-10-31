# Initial BLE protocol and implementation plan

Draft for discussion, October 26, 2026; revised for an **nRF52 peripheral, Android/iOS receivers, and 30 Hz foreground telemetry**. **Planning only: no BLE or firmware implementation is included.** Existing serial behavior is documented separately in [Serial_Message_Reference.md](Serial_Message_Reference.md); its current approximately 2 Hz output is not the proposed BLE rate.

## 1. Topology and scope

Interpretation of the multiple-device requirement: a phone must select a particular tester among several nearby devices, so measurements must not depend on one shared open broadcast. Use connectable advertisements for discovery and a separate, encrypted **GATT connection** for each selected tester. The nRF52 tester is the peripheral/GATT server; an Android or iOS app is the central/GATT client. Use the native Android BLE or iOS Core Bluetooth interface directly, or a phone framework that exposes the same connection, notification and write capabilities. This is a custom GATT protocol, not Bluetooth Classic serial/SPP.

Initial policy: **one connected phone per tester**, with up to four saved phone bonds. A phone may connect to several testers, subject to its tested platform limits; start acceptance testing with four simultaneous tester connections. Multiple nearby testers do not require multiple simultaneous centrals on each tester. Supporting spectators on one tester is a later feature.

Phase one provides read-only monitoring, explicit identity/mode, cable/weapon/probe readings, battery health, and reliable recent weapon events. Remote mode changes are a later opt-in feature. Capture transfer is a separate later phase. Keep local testing functional without a phone and preserve the USB diagnostic channel.

The active measurement target is **30 complete snapshots per second per selected tester**, approximately one every 33.333 ms, with one notification per snapshot on the preferred MTU path. Use a custom binary Telemetry characteristic with **64-byte maximum value length**, plus separate control and result/event characteristics. Display the newest received state at 30 Hz or the next available screen frame; do not buffer a long history in the live rendering path. This target applies to foreground monitoring. Background collection and OS suspension need separate platform testing and do not carry a 30 Hz display guarantee.

## 2. Discovery and device identity

Allocate a custom 128-bit service UUID and a stable 64-bit application device ID during provisioning; check IDs for duplicates. The full ID is readable after connection and shown on the tester's identity screen or label/QR. Never use the advertised name, RSSI, or Bluetooth address as the authoritative identity; addresses can change and short names can collide.

Use legacy advertising initially for compatibility. A draft layout fitting the 31-byte advertising limit is:

| AD element | Bytes including length/type | Contents |
| --- | --- | --- |
| Flags | 3 | Standard LE flags |
| Complete 128-bit service UUID list | 18 | One custom tester service UUID |
| Service Data, 128-bit UUID | Not included | Would exceed this layout's budget |

Put a shortened/complete local name such as `TTArm-A1B2C3` in scan response (13 bytes including AD overhead). The suffix is a discovery hint from the device ID, not unique proof. Read full identity and protocol version on connection. Additional identity advertising data can be considered later; do not invent a manufacturer company ID.

For Apple-compatible discovery, propose 20 ms advertising for the first 30 seconds of the 60-second pairing window, then 152.5 ms for the rest; normal reconnect advertising uses 546.25 ms. These are from [Apple's accessory parameter guidance](https://developer.apple.com/library/archive/qa/qa1931/_index.html), which also documents discovery/connection tradeoffs; validate against the current accessory guidance at implementation. Stop connectable advertising while the one-phone slot is occupied. The app marks an already connected/previously seen tester using its own state; absence from scan alone does not prove that a tester is busy.

The picker displays friendly name, short/full known ID, saved alias, bond state, and RSSI as a proximity hint. Selecting a new tester leads to physical identity confirmation during pairing. All app parsers, queues, subscriptions, reconnection attempts, and UI panels are scoped by device ID plus connection/session, preventing readings from crossing between testers.

## 3. Pairing, bonding, and ownership

1. A dedicated local settings action opens a 60-second pairing window and displays the tester's ID. Reuse the menu rather than overloading existing long-press calibration/power-off behavior.
2. The user selects that tester in the app, which connects and requests security through the phone's platform BLE flow. Do not assume Android and iOS present identical pairing screens or require prior pairing in system settings.
3. Prefer **LE Secure Connections with authenticated passkey entry**: tester displays a fresh random six-digit code, entered on the phone. This suits an existing display without needing a keyboard on the tester. Configure display-only I/O. Require authenticated encryption for measurement reads/subscriptions and command writes; pairing mode alone must not grant measurement access.
4. Bond keys are saved on both sides. Reconnects use the saved bond and encryption without reentering a code. Reject new pairing outside the local window, and reject silent security downgrade. Validate actual Secure Connections support on both nRF52832 and nRF52840 board/core combinations before committing to this requirement. If it is unavailable, revise the supported platform/security policy explicitly; do not silently fall back to Just Works.
5. Initial deployment is first-bond enrollment. Adding/replacing another phone requires a new local pairing action; capacity-full returns a visible error rather than silently evicting a bond. The authenticated connected phone has control ownership; no unauthenticated observer mode in v1.
6. Local settings provide forget-one/forget-all bonds with on-device confirmation. Document removal on the phone too. Lost-phone recovery is available locally without the lost phone. Failed bond restoration prompts explicit repair through this local process.

Unauthenticated access is limited to discovery and minimal product/protocol identity. Use a fresh code per pairing attempt and rate-limit failures. Bonding and application identity are separate: the stable ID selects the UI record, and the stored BLE keys authenticate the peer. No static universal PIN. See [Bluefruit bonding guidance](https://learn.adafruit.com/bluefruit-nrf52-feather-learning-guide?view=all) and [Bluefruit security implementation](https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/libraries/Bluefruit52Lib/src/BLESecurity.cpp). Core configuration and supported targets still require a local integration audit.

## 4. Proposed GATT service

Assign one vendor UUID family, with distinct characteristic UUIDs; freeze actual UUID values in the final protocol specification. These names are roles, not Bluetooth assigned numbers.

| Characteristic | Properties | Role |
| --- | --- | --- |
| Protocol Identity | Read | Protocol major/minor, firmware/build, model, board revision, full device ID, capability bits, boot ID |
| Telemetry | Notify; variable length, maximum 64 bytes | One active-mode measurement snapshot at 30 Hz, plus slower system status |
| Control | Write with response; variable length, maximum 64 bytes per fragment | Requests: hello, get snapshot, subscription profile, identify; later explicit remote mode changes |
| Result/Event | Indicate; variable length, maximum 64 bytes per fragment | Command results, state changes, weapon events and overflow reports |
| Capture | Notify; maximum 64 bytes per fragment, optional later | Requested capture manifests and chunks, paced below live telemetry |

Minimal unauthenticated identity can expose product and protocol major only. Full identity including device ID/boot ID is obtained after encryption, using bounded reads/long reads as needed. Add standard Device Information Service; add standard Battery Service only if a validated percentage estimate exists. Millivolts in custom telemetry are sufficient for v1.

Use notifications for replaceable measurements. Indications confirm GATT receipt, not app persistence; event retention and application ACKs remain necessary. Debug prose stays on USB and never shares the binary characteristic. The Bluetooth specification defines the distinction between notification and indication and the ATT MTU rules: [GATT specification](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/host/generic-attribute-profile--gatt-.html).

**Characteristic size and MTU are separate settings.** A 64-byte characteristic is a storage/value limit, not permission to send 64 bytes on every link. Plan an nRF52 ATT MTU ceiling of 247, subject to installed core/SoftDevice support, and negotiate per connection. MTU 67 is sufficient for the whole 64-byte characteristic value; the largest current measurement frame is only 48 bytes and requires MTU 51. Reserve the remaining space for future compatible packet types rather than padding every notification. A 512-byte Telemetry characteristic is unnecessary for the proposed live UI. In an Adafruit implementation, variable maximum length, peripheral bandwidth/MTU, connection-event resources and notification queue depth must all be configured; [Adafruit's Bluefruit guide](https://learn.adafruit.com/bluefruit-nrf52-feather-learning-guide?view=all) describes these as separate controls. A larger ATT value may still span multiple link-layer packets; Data Length Extension is an optimization, not a protocol requirement. Keep 1M PHY compatibility rather than requiring 2M PHY across all nRF52 variants.

## 5. Packet scheme, version 1 draft

### Encoding and logical message header

All integers are little-endian; sizes below are bytes. No packed C struct is the protocol contract. Encode fields explicitly in future implementation. Maximum logical message size is 512 bytes including header. Unknown major versions reject the session; minor versions may add negotiated optional message types. Unknown types are skipped/rejected by length without changing device state.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | Protocol major, initially 1 |
| 1 | 1 | Message type |
| 2 | 1 | Flags: bit 0 replay; remaining bits reserved zero |
| 3 | 1 | Current device mode, using the mode enumeration below; `0xFF` in phone requests |
| 4 | 2 | Sequence number, modulo 65536, per direction and characteristic |
| 6 | 4 | Device uptime in milliseconds, unsigned modulo 2^32; zero in phone requests |
| 10 | 2 | Payload length, 0–500 |
| 12 | variable | Type-specific payload |

Full identity supplies a random 64-bit boot ID generated once per boot. Phone state is keyed by device ID + boot ID + BLE session. Sequence numbers detect omissions/duplicates within that scope, and must never be used alone as device identity or proof of reboot. Reconnect resets transport sequence/reassembly state; retained event IDs are separate. No application CRC for ordinary connected messages; capture export can add CRC32 to validate a complete reconstructed dataset.

### Fragmentation for the default MTU

Support ATT MTU 23, which allows 20 bytes of value in an ordinary notification/write, and prefer larger MTUs for 30 Hz. For these characteristics the actual value limit is **min(64, negotiated ATT MTU minus 3)**. Do not assume a requested MTU is granted. This default and payload limit are described in [TI's BLE stack guide](https://software-dl.ti.com/lprf/simplelink_cc2640r2_sdk/1.00.00.22/exports/docs/blestack/html/ble-stack/index.html?highlight=gapbondmgr).

Every streamed/control value, even a single-fragment message, has this six-byte transport wrapper, followed by a slice of the logical message:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 2 | Transport message ID, modulo 65536 per direction/characteristic |
| 2 | 1 | Zero-based fragment index |
| 3 | 1 | Fragment count, 1–255 |
| 4 | 2 | Total logical message length, 12–512 |
| 6 | variable | Consecutive message bytes |

The logical slice limit is **min(64, negotiated ATT MTU minus 3) minus 6**. At MTU 23 it is 14 bytes; a 512-byte message needs 37 fragments. At MTU 67, 185 or 247 it is 58 bytes because this plan caps each characteristic value at 64 bytes; a 512-byte message needs nine fragments. Logical-message length is therefore distinct from individual characteristic-value length. Fill every nonfinal fragment to the negotiated slice size. Require consistent ID/count/length and exact final reassembled size. One in-flight message per direction/characteristic, fragments in order; reject missing, duplicate, inconsistent or excessive fragments. Bound allocation at 512 bytes before copying. Firmware queues indications one at a time and handles BLE backpressure without blocking ADC work.

For 30 Hz Telemetry, discard incomplete reassembly after **100 ms**, on a newer message ID's fragment zero, or on disconnect. Never interleave fragments from different snapshots: complete the active small frame or discard it before starting the newest one. Other control/result/capture characteristics retain a 2-second reassembly timeout. This keeps an incomplete old measurement from blocking the live display.

Identity reads use GATT read/long-read semantics, not this fragment wrapper. All other listed characteristics use the wrapper. An ATT write response only acknowledges a fragment's acceptance; a Result message acknowledges complete request validation/execution.

### Initial payloads

Field order below is wire order; scalar widths are explicit. No implicit padding.

| Type | Payload |
| --- | --- |
| `0x01` Status | `mode:u8, state_flags:u16, battery_mV:u16, validity:u16, uptime_sample_count:u32, diagnostic_rate_Hz:u32` (15 bytes) |
| `0x10` Cable | `snapshot_id:u16, fault_mask:u16, validity:u16, AA:i32, AA_peak:i32, BB:i32, BB_peak:i32, CC:i32, CC_peak:i32` (30 bytes) |
| `0x11` Cable ADC | `snapshot_id:u16, validity:u16, AA:u16, AB:u16, AC:u16, BA:u16, BB:u16, BC:u16, CA:u16, CB:u16, CC:u16` (22 bytes) |
| `0x12` Weapon | `snapshot_id:u16, validity:u16, state_flags:u8, E:i32, F:i32, last_epee_edge_ms:u32, last_foil_edge_ms:u32` (21 bytes) |
| `0x13` Probe | `snapshot_id:u16, validity:u16, E:i32, F:i32, WpnAC:i32, APr:i32, BPr:i32, CPr:i32` (28 bytes) |
| `0x20` Weapon event | `event_id:u32, weapon:u8, event_kind:u8, edge_time_ms:u32, duration_us:u32` (14 bytes) |
| `0x21` State event | `event_id:u32, old_mode:u8, new_mode:u8, reason:u8` (7 bytes) |
| `0x22` Overflow | `event_id:u32, first_lost_event_id:u32, last_lost_event_id:u32` (12 bytes) |
| `0x30` Request | `request_id:u32, opcode:u8, arguments:bytes` |
| `0x31` Result | `request_id:u32, opcode:u8, result:u8, detail:bytes` |

All resistance integers are **milliohms**: 420 means 0.420 ohms. Reserve `INT32_MAX` for open circuit and `INT32_MIN` for invalid/unavailable. Preserve signed near-zero calibrated values. ADC `0xFFFF` means unavailable. Battery `0xFFFF` means unavailable. The explicit sentinels replace the ambiguous legacy `999.9` numeric convention.

Mode values: 0 cable, 1 digital weapon, 2 weapon resistance, 3 hit capture, 4 probe, 5 idle, 6 off. Status state flags: bit 0 cable disconnected, 1 weapon disconnected, 2 lame mode, 3 mask mode, 4 capture armed, 5 capture ready; other bits zero. Status validity bit 0 validates battery and bit 1 validates rate/count. Rate/count must have a documented mode-specific meaning and must not be used for waveform timing.

Cable validity bits 0–5 cover the six resistance fields in listed order. Cable ADC bits 0–8 cover its nine fields. Weapon validity bits 0/1 cover E/F, 2/3 cover edge timestamps, and 4 validates state flags; weapon state bits 0/1 are epee/foil input-active, 2/3 intermittent indicators, 4 AC short. Probe validity bits 0–5 cover its listed resistances. Unset validity overrides the corresponding value; reserved bits are zero. Cable faults retain the existing nine-bit map documented in the serial reference. Faults and numeric values come from one coherent snapshot.

Snapshot IDs group related messages from one published measurement state; telemetry header time is the snapshot publication time. Header byte 3 supplies mode on every message, so the phone can distinguish weapon resistance from hit-capture mode immediately without waiting for slower Status or an event. Snapshot publication must draw from the newest available completed acquisition; separately measure acquisition age, because publication time alone cannot prove fresh data. Raw ADC is optional diagnostics. A mode change cancels pending old-mode snapshots. Inapplicable fields are invalid, never reused from an earlier mode.

### Notification sizes and 30 Hz bandwidth

The normal stream sends just the measurement type appropriate to the active mode: Cable, Weapon (including digital mode, with resistances invalid when unavailable), or Probe. Status is additional low-rate traffic. The six-byte transport wrapper plus 12-byte logical header adds 18 bytes per unfragmented value.

| Message | Payload | Complete unfragmented value | Minimum ATT MTU | Notifications per snapshot at MTU 23 |
| --- | --- | --- | --- | --- |
| Cable | 30 bytes | 48 bytes | 51 | 3 |
| Weapon | 21 bytes | 39 bytes | 42 | 3 |
| Probe | 28 bytes | 46 bytes | 49 | 3 |
| Cable ADC (optional) | 22 bytes | 40 bytes | 43 | 3 |
| Status (slow) | 15 bytes | 33 bytes | 36 | 2 |

For cable mode on MTU >=51: 48 bytes x 30 = **1440 characteristic-value bytes/second per tester**, excluding slow status/events and lower-layer overhead. Four testers total 5760 bytes/second and 120 measurement notifications/second. At MTU 23 the 42 logical Cable bytes require three 14-byte slices plus three wrappers: 60 bytes/snapshot, **1800 bytes/second and 90 notifications/second per tester**. Four testers then require 360 measurement notifications/second. The byte rate is modest; scheduling, queue behavior and fragment completion latency are the key risks. These are protocol calculations, not measured performance guarantees.

Weapon event enumeration proposal: weapon 0 epee, 1 foil; kind 0 input inactive edge, 1 input active edge, 2 qualified activation, 3 qualified release. `duration_us=0xFFFFFFFF` means unknown/not applicable. Preserve physical input semantics separately from any fencing-rule interpretation. Timestamp events when detected, not when transmitted. Event-generation semantics/debounce must be reviewed against the existing ISR and polling behavior before implementation; legacy trigger timestamps alone cannot create a complete event log.

### Request/response rules

Initial opcodes: `0x01 HELLO` (supported major/minor), `0x02 GET_SNAPSHOT` (no arguments), `0x03 SET_STREAM` (profile:u8, measurement_rate_Hz:u8, adc_rate_Hz:u8), `0x04 IDENTIFY` (duration_ms:u16, maximum 5000), `0x05 ACK_EVENTS` (last_contiguous_event_id:u32), `0x06 REPLAY_EVENTS` (first_event_id:u32). Mode change `0x10 SET_MODE` remains an unadvertised optional capability until separately implemented and tested.

Profiles: 0 stopped (both rates zero), 1 normal (ADC rate zero), 2 diagnostic including ADC. Supported active measurement rates are 1, 2, 5, 10, 15 and **30 Hz; default normal profile is 30 Hz**. Diagnostic ADC rate is independently 1, 2 or 5 Hz, default 2 Hz; do not double 30 Hz traffic just to expose debugging values. Return accepted profile and both rates. Generate exactly 30 Hz with a microsecond phase accumulator or equivalent absolute-deadline schedule: intervals of 33333, 33333 and 33334 microseconds average 33.333 ms. A fixed 33 ms interval is approximately 30.30 Hz. Do not send catch-up bursts after a stall; skip expired deadlines and publish the newest state.

HELLO returns selected version, capability mask, boot ID, the peripheral's current effective fragment-value limit (20–64 bytes) and maximum supported measurement rate. The phone uses the smaller of the returned limit and its applicable platform write limit for Control. GET_SNAPSHOT returns success plus schedules Status and applicable measurement types. Every complete request produces a Result: 0 OK, 1 malformed, 2 unsupported, 3 unauthorized, 4 busy, 5 invalid state, 6 out of range, 7 history unavailable. Payload sizes for each opcode are exact; reject extras/truncation and leave settings unchanged. Do not silently convert a 30 Hz request into a lower rate; report overload/degraded delivery separately from the accepted publication rate.

Use monotonically assigned request IDs within a connection and one outstanding request. Cache the last eight completed request/result pairs, including argument bytes, for that connection. Retrying identical ID and arguments returns the cached result without repeating an action; same ID with different arguments is malformed. Phone times out after 3 seconds and retries twice. Clear the cache on disconnect; on reconnect query current state before reissuing state-changing actions. Do not replay power-off, calibration, or persistent writes automatically; these are outside initial scope.

## 6. Streaming, retention, and recovery

After encrypted connection: read identity, discover/configure the effective MTU, subscribe to Result/Event and Telemetry, send HELLO, set stream profile, request snapshot. Keep high-rate telemetry stopped until these steps complete; use the default 20-byte value limit for initial HELLO requests if the limit is not yet known. Reestablish subscriptions on every connection and verify identity/boot ID before accepting queued data into the selected tester's UI.

Publish active-mode measurement snapshots at **30 Hz**, independently of USB serial timing, local display drawing and battery sampling. Status is sent every five seconds and on mode changes, with battery at its own acquisition rate. Do not poll the characteristic with 30 reads/second; subscribe to notifications. Raw ADC diagnostics remain at their separately selected lower rate. Test four connected testers, each at 30 Hz; no aggregate phone scheduling guarantee is assumed. Idle mode may send Status alone and explicitly report that measurement streaming is inactive.

### Android and iOS receiver behavior

**Android:** request ATT MTU 247 and use the actual `onMtuChanged` result, not the requested value. Android 14 and later request 517 for the first GATT MTU request; an nRF52 ceiling of 247 can still negotiate a smaller result. During active foreground monitoring, evaluate/request high connection priority and return to balanced priority after streaming stops. Subscribe locally and write the CCCD, waiting for completion before enabling the stream. Serialize GATT setup/control operations and decode incoming notification values off the UI thread. These MTU/priority behaviors are documented in the [Android BluetoothGatt API](https://developer.android.com/reference/android/bluetooth/BluetoothGatt).

**iOS:** use Core Bluetooth notification subscription and its completion callback before starting the stream. iOS chooses its MTU; do not assume an Android-style request-MTU API, a fixed 185-byte MTU, or direct app control of the connection interval. The nRF52 reads its negotiated ATT limit and reports the effective value limit in HELLO; the iOS app also consults `maximumWriteValueLength(for:)` for the applicable Control write type. That API is a write limit, not a notification-MTU getter. For this protocol, cap writes to min(platform limit, reported value limit, 64) and use the same fragment scheme. See [Apple's Core Bluetooth session](https://devstreaming-cdn.apple.com/videos/wwdc/2017/712jqzhsxoww3zn/712/712_whats_new_in_core_bluetooth.pdf) and [maximumWriteValueLength](https://developer.apple.com/documentation/corebluetooth/cbperipheral/maximumwritevaluelength(for:)).

Both apps should retain only the latest completed snapshot for rendering and update graphs/labels at 30 Hz or the next screen frame. Keep an optional bounded history for graphing/logging separate from the rendering queue. Receipt callbacks may arrive in batches; never replay those batches as delayed UI frames. Mark live telemetry delayed after 100 ms without a complete active-mode snapshot, stale after 250 ms, and disconnected when the BLE session ends. Rate-limit UI work and avoid new control writes per frame. Background behavior is a separate acceptance case, not a promise of continuous 30 Hz delivery or rendering.

### Connection timing and latency budget

Request a **15–30 ms connection interval**, peripheral/slave latency **0**, supervision timeout **4 seconds** for active streaming. In BLE units that is interval min 12, max 24 (1.25 ms units), and timeout 400 (10 ms units). The central decides the accepted interval; record the actual result. Apple's documented parameter relationships permit this request, but do not guarantee acceptance on every device: [Apple accessory connection parameters](https://developer.apple.com/library/archive/qa/qa1931/_index.html). Do not make 7.5 ms intervals, 2M PHY, or a particular packets-per-event count a cross-platform requirement. If the phone chooses a slower link, measure and surface degraded delivery rather than building a queue of old frames.

Proposed foreground acceptance target: **95th-percentile change-to-visible-update latency <=100 ms** for an unsmoothed input/state, with a 30 Hz source and no sustained backlog. The nominal budget is up to 33.333 ms awaiting publication, up to 30 ms awaiting a connection event, plus phone callback/render time. Retries, fragmented frames and OS scheduling can exceed that budget; this is a target to validate, not a hard real-time guarantee. Timestamp digital edges when detected and queue their event indication immediately, independently of the next 30 Hz measurement deadline. Short input pulses still need retained events because snapshots alone can miss them.

Publishing at 30 Hz does not remove latency already introduced by acquisition, decimation, cable low-pass filtering or peak decay. The current cable path uses a low-pass filter identified as 3 Hz and a nominal 200 ms peak decay constant; characterize its step response separately. Measure both acquisition-to-screen transport latency and physical-input-to-screen latency; do not claim the latter meets 100 ms until the filtering and trigger paths have been measured. A deliberate smoothed resistance display may need a different response target from an immediate input/fault indicator.

When backpressured, retain at most one unsent newest snapshot, coalesce/drop older periodic telemetry, and record drop counters. Tune the nRF52 notification queue to avoid more than about one 33 ms frame of queued live data; multiple entries may be needed to complete an MTU-23 fragmented snapshot within a connection event. Do not block ADC work or the main loop waiting for transmit credits. Slow or pause diagnostics/capture first. Reserve service opportunities for both live telemetry and event/control traffic; bound event replay bursts so historical events cannot starve 30 Hz updates. Events use a bounded 64-entry RAM ring, numbered monotonically per boot. Keep entries until acknowledged by ACK_EVENTS; indications alone do not remove them. On reconnect in the same boot, request replay from the last saved contiguous event ID. App stores/deduplicates events before ACK. Sequence wrap uses modular comparison, with retained windows much smaller than half the ID range.

On overflow, track the lost ID range outside the ring and send an Overflow report when the link permits; never silently claim complete history. If requested history was evicted or rebooted, return history unavailable and obtain a fresh snapshot. No persistent event archive in v1. Serialize result/event delivery; reserve queue space for command replies and overflow status so events cannot starve control.

A future capture manifest must identify capture ID, boot ID, weapon/channel, sample count, trigger index, actual timebase or per-sample timing, ADC scale/trim/range, and checksum. Transfer chunks carry capture ID, starting sample index and explicit sample count. Pin or copy the shared capture buffer during transfer, support bounded offset retries, and never infer sampling interval from the serial heartbeat rate. Existing epee/foil capture buffers share storage, making ownership and overwrite prevention a required design step.

Reconnect with exponential backoff and jitter (about 1–30 seconds), scoped to the selected known tester. Never automatically pair with the nearest same-name device. Show disconnected/stale states promptly; reconnect does not revive cached values. Keep normal local power-off policy unless an explicit later setting changes it. BLE connection supervision timeout and app stale timers are independent; reapply the active 15–30 ms interval preference and 30 Hz stream setup after each reconnection. When monitoring stops, stop high-rate telemetry and allow lower-power connection parameters.

## 7. Implementation stages and acceptance gates

1. **Platform audit:** identify installed Adafruit core, SoftDevice, bootloader and memory map for both MCU targets. Check BLE stack IRQ priority/reserved-resource requirements against direct SAADC/NVIC/GPIOTE operations, ADC ISR execution time, flash/filesystem writes, display SPI, FastLED timing, and watchdog. The current app has no active Bluefruit setup. Baseline measurement accuracy, capture timing, throughput, RAM and power before integration. Decide whether both targets meet authenticated Secure Connections and buffer budgets.
2. **Freeze contract:** allocate UUIDs, define identity encoding/capability bits and remaining result details, record actual binary packet fixtures, and agree event semantics and expiry/ownership policies. Maintain a separate parser/UI model from USB text. Size total RAM including BLE stack, event ring, fragments and existing capture/display buffers; 52832 is the tighter target.
3. **Discovery/security:** implement per-device identity, local pairing UI, authenticated bonding, bond management and reconnect. Test two same-name devices and wrong-code attempts before adding measurements.
4. **Read-only protocol:** configure the custom 64-byte characteristics and nRF52 MTU/queue resources; implement bounded serialization/reassembly outside interrupts, coherent snapshots, all current modes, validity/sentinels, 30 Hz absolute-deadline publication and diagnostic backpressure. Mirror serial measurement semantics where appropriate; source probe/mode/validity directly from device state. Decouple BLE scheduling from the existing 500 ms serial schedule and blocking display/filesystem work. Validate the MTU-23 fallback as well as single-notification snapshots on Android and iOS.
5. **Events and phone integration:** implement event capture/queue/ACK/replay, overflow reporting, device-scoped app state, stale handling and lifecycle recovery. Add optional remote mode control only after local/remote arbitration is specified; local button wins, device reports final state, invalid mode transitions return errors.
6. **Capture phase:** define/freeze manifest and chunk encodings, ensure shared buffer ownership, then validate full sample reconstruction and interruption/retry behavior. This phase is not required for initial read-only release.

Acceptance tests before release: four testers nearby and connected to one phone, all publishing at 30 Hz; identical short names; wrong-target selection; two phones competing for one tester; full bond store; lost/stale bonds; wrong PIN and downgrade rejection; representative Android vendors/OS versions and iPhone/iOS versions; reconnect and background/foreground behavior; MTU 23, the minimum unfragmented MTU 51 and larger MTUs; actual connection intervals 15/30 ms and slower central-selected intervals; malformed/oversized/missing fragments; duplicated commands; event bursts/overflow/replay; cable opens/shorts and bit 8; probe and idle mode; reboot, uptime/sequence wrap; disconnect during command/capture; low battery; ADC/trigger accuracy and watchdog behavior under display and BLE load.

Run at least ten minutes of foreground streaming per phone/tester configuration. Initial pass criteria: mean **29.7–30.3 complete measurement snapshots/second per active tester**, at least **99%** of scheduled snapshots delivered complete, 95th-percentile unsmoothed change-to-screen latency **<=100 ms**, no continuously growing transmit/render queue, and accurate drop accounting. Identify radio conditions, MTU, actual interval, number of testers and phone model for each result. If a small-MTU or multi-device configuration cannot meet these criteria, document it as degraded/unsupported for the 30 Hz target rather than reporting the requested rate as delivered. Verify battery overhead and measurement/filter response separately; agree remaining power/accuracy/reconnect limits before declaring the protocol stable.

Open decisions for implementation review: exact nRF52 variant and supported core/SoftDevice/security configuration; Android/iOS minimum supported versions; validated maximum concurrently monitored testers per phone at 30 Hz; event retention and remaining latency requirements; UUID and identity provisioning; background monitoring requirements; and whether remote controls/capture belong in the first product release. The 64-byte characteristic and 30 Hz foreground publication target are the current design baseline; no firmware changes are part of this document update.
