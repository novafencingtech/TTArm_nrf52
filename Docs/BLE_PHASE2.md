# BLE Phase 2: diagnostic Status and target MTU

Current Status wire format is protocol 2.1; see [app sync and byte vectors](BLE_STATUS.md).

Phase 2 publishes the specification's diagnostic Status message. Cable was added subsequently; Weapon,
Probe, Lights, Capture and remote command execution remain disabled. Standard
Just Works encryption and both nRF52832/nRF52840 support are retained.

## Immediate phone test

1. Upload the build for the physical board and allow the normal measurement loop
   to run for at least five seconds. Diagnostics initialize through the existing
   serial heartbeat, not through BLE. If settings were opened immediately after
   boot, exit once to let that calculation run.
2. Connect to `TTArm-XXXXXX` and complete pairing/reconnection. In **Bluetooth >
   Status**, `MTU: actual / 247` shows the observed ATT MTU and the target. The
   firmware requests 247 on connection and retries after encryption if the
   request was temporarily busy. The central may accept a smaller value.
3. Enable notifications (`01 00` in the CCCD) on **Status**:
   `4a1e0020-3bb5-44b4-b566-8d22e72da6f5`.
4. Expect an immediate complete Status once initialized, then approximately one
   per second. Actual mode transitions also request immediate Status. Other
   characteristic subscriptions do not enable it. Disabling its CCCD stops it.
5. At MTU 247 a notification value is **36 bytes**, not 66 or 244. At MTU 23 one
   Status is three values of **20, 20, and 8 bytes**. Reassemble those before decoding.

To force a fallback test, use a client/link that stays at MTU 23, or temporarily
set `TTARM_BLE_TARGET_MTU` to 23 in `BleConfig.h`. No compiler override is needed.
The default is 247, configured with the stock runtime API before stack startup.
Event length and notification queue depth remain small; DLE is not assumed.

## Bytes and meaning

All integers are little-endian. Each notification starts with the six-byte
fragment wrapper from specification section 6. The logical message is exactly
30 bytes: the 14-byte live header followed by the 16-byte Status payload.

For an unfragmented 36-byte notification:

| Value offset | Field |
| --- | --- |
| 0..5 | Transport ID, fragment index/count, logical length (30) |
| 6 | Protocol major (2) |
| 7 | Status message type (1) |
| 8 | Flags: 2 for immediate update, 0 for periodic |
| 9 | Current protocol mode |
| 10..11 | Connection-local Status sequence |
| 12..15 | Publication uptime in milliseconds |
| 16..17 | Payload length (16) |
| 18..19 | Boot-local mode epoch |
| 20..23 | Existing serial diagnostic interval count N |
| 24..27 | Existing serial rate, rounded to whole Hz |
| 28..29 | Last valid measured battery millivolts |
| 30..33 | Average loop-start interval, microseconds (0 = waiting) |
| 34..35 | Negotiated connection interval, 1.25 ms units (0 = unknown) |

N is not a lifetime acquisition count, and rate is not notification rate. The
existing serial calculation updates about every five seconds and battery about
every ten seconds. Status repeats those cached results between updates. Header
uptime describes publication, not a new battery acquisition. Menus/calibration
can pause the existing calculations; BLE does not introduce another schedule.

The serial path snapshots N once and reuses its existing formatted whole-Hz
rate, preserving its rounding. The BLE hook runs after the existing N reset;
it never performs a second reset. Battery is cached only after real acquisition,
not from the startup default. Invalid/nonfinite/out-of-range voltages do not
replace a valid cache. Zero count/rate or zero measured millivolts are valid.
No notification is formed until both the count/rate and battery are initialized.

Battery sampling now saves/restores the actual ADC interrupt/scan state, including
Probe and Idle, instead of assuming only Cable/Weapon can be scanning. No extra
battery acquisition is added.

Full Identity now advertises capability `0x00000201` (Cable plus battery diagnostics). Other
measurement and command capability bits remain zero. Identity stays immutable
throughout the connection, including while MTU negotiation changes capacity.

## Sender behavior

- One immutable 30-byte logical frame is pending at most. There is no history
  queue or dynamic allocation. Each foreground poll attempts at most three packets.
- The stock SoftDevice HVX call returns immediately on backpressure. A rejected
  fragment is retried without advancing its index or changing its bytes. Unlike
  Bluefruit's convenience `notify()`, this path never waits for a queue credit.
- Fragment boundaries are fixed at frame creation using the actual ATT MTU,
  capped at a 66-byte value. An MTU increase affects only the next frame. A
  capacity decrease that invalidates a pending frame reestablishes the session.
- A pending Status expires two seconds after formation. Replacement uses the
  newest cache; there is no catch-up burst or unbounded backlog.
- Disabling notifications, losing encryption, disconnecting, or changing mode
  drops incompatible pending data. Restored bonded CCCDs are checked after
  encryption. Accepted CCCD toggles are observed synchronously.
- Sequence and transport IDs start at zero on each connection and wrap at
  65536. Retries reuse them. Mode epoch increments only on actual BoxState
  changes, including direct setters/calibration, and survives reconnects.
- Shutdown requests a bounded best-effort Off Status before stopping the link;
  it does not wait for transmission or delay power-off.

## Historical Phase 2 build and validation

The results below predate the 2.1 timing additions; they are not validation of the new fields.

Use the ordinary Arduino IDE build or the stock CLI helper; no core patch or
special compiler flags are required:

```powershell
python tools/build_ble.py                         # nRF52832
python tools/build_ble.py --board feather52840
python tools/test_ble.py
```

Portable tests include exact byte vectors, initialization gates, battery bounds,
MTU 23/24/32/33/69/185/247, immutable retries, MTU changes mid-frame, two-second
expiry, mode transitions, CCCD toggles, reconnect resets, and counter/uptime wrap.
The SDK adapter is checked with simulated resource, MTU-in-progress, security,
disconnect and data-size errors. Existing pairing/menu/GATT tests are retained.

Verified October 31, 2026 with the unmodified Adafruit nRF52 core 1.3.0 and
Bluefruit 0.21.0. Both enabled builds match the working source snapshots and use
the stock library search and compiler properties.

| Check | Result | Flash bytes | Static RAM bytes |
| --- | --- | --- | --- |
| nRF52832, MTU target 247 | Pass | 178,616 | 29,672 |
| nRF52840, MTU target 247 | Pass | 227,728 | 34,836 |
| nRF52832, radio off | Pass | 158,744 | 27,612 |
| Portable pairing/GATT/UI and Status tests | Pass | — | — |

Sizes are compiler reports rather than runtime heap/stack measurements. Existing
CMSIS/Arduino `PI` and FastLED nRF52832 warnings remain.

On November 1, 2026, the user confirmed that Status messages are working on
hardware. Negotiated MTU, restored subscriptions, serial-value comparison, and
ADC/capture timing under active radio traffic still require explicit validation.
No hardware is automatically flashed by the helpers.
