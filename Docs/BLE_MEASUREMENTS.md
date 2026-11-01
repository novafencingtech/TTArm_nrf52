# Weapon, Probe and Lights messaging

Implements protocol 2.1 Weapon, Probe and Lights alongside Cable and Status.
Full Identity advertises capability mask `0x0000020F`. Commands and Capture
waveform transfer remain deferred. Local Capture mode publishes live Weapon data.

| Stream | Characteristic UUID suffix | Protocol modes | Logical / wrapped bytes |
| --- | --- | --- | --- |
| Weapon | `0011` | 2 Weapon resistance, 3 local Capture | 33 / 39 |
| Probe | `0013` | 4 Probe | 40 / 46 |
| Lights | `0014` | 1 digital Weapon Test | 26 / 32 |

The common UUID form is `4a1eXXXX-3bb5-44b4-b566-8d22e72da6f5`.
Each stream requires an encrypted connection and its own notification CCCD.
Status and Cable subscriptions remain independent.

Weapon and Probe use absolute 30 Hz deadlines with missed slots skipped.
Foreground hooks cache existing completed state without additional acquisition.
Weapon includes both resistances, active/latch flags, AC short and accepted
edge times. Edge times use the existing last accepted transition timestamps
(`tEpeeInterOn` / `tFoilInterOn`), rather than IRQ debounce trigger timestamps.
Probe explicitly orders its six resistances as E/F/WpnAC/APr/BPr/CPr; acquisition
stores BPr before APr, so the hook reorders those two fields.
Signed milliohms and OPEN use the same conversion policy as Cable. Nonfinite or
out-of-range values withhold publication. Mode entry waits for fresh channel
completions before populating its cache.

The digital Weapon Test handler runs in the foreground. Each accepted active
or latch change copies its full state and accepted time into the Lights
publisher under the manager mutex. GPIO interrupt handlers remain unchanged;
they provide debounce triggers, not accepted UI states. A bounded 16-state FIFO
preserves change order normally; overflow discards old queued states and keeps
the newest. Queued states older than 100 ms collapse to the latest full state.
Every new accepted change increments the boot-local snapshot counter, including
changes while unsubscribed. An initial mode/subscription copy publishes the
latest state immediately; old queued events are cleared on CCCD changes,
mode transitions and reconnect. Quiet Lights publishes a full-state refresh
at 1 Hz, reusing the snapshot ID and edge times with a new sequence/time.
No event history or lossless delivery is promised.

All transports bound work per poll and retry without waiting for BLE credits.
Retries preserve payload bytes and fragment boundaries, even if MTU grows.
An MTU shrink below the pending frame's limit requests reconnect. Pending
frames expire after 100 ms. Snapshot IDs persist across modes/reconnect;
sequence and transport IDs restart per connection. Mode epochs discard old
fragments and cached state. Each publisher owns less than 512 bytes of state.
At default MTU 23, Weapon/Probe/Lights require 3/3/2 notifications; at MTU 247,
each complete message fits in one notification.

## Validation

Run `python tools/test_ble.py` for portable codec, lifecycle, fragmentation,
queue overflow/expiry, refresh, deadline and nonblocking GATT routing checks.
Run `python tools/build_ble.py --board feather52832` and the corresponding
`feather52840` build; add `--ble-disabled` for the radio-off configurations.

On November 1, 2026, all portable BLE tests passed, and standard Arduino builds
passed for nRF52832 and nRF52840 with BLE enabled and disabled. The BLE-enabled
nRF52832 build uses 188360 bytes flash and 31144 bytes static RAM (21080 bytes
remaining); nRF52840 uses 237472 bytes flash and 36308 bytes static RAM.
Existing PI redefinition and FastLED board warnings remain.

Hardware qualification remains pending: compare all fields with local display
and electrical inputs; exercise both weapons, latch hold/clear, Probe A/B
ordering, OPEN and negative offsets, mode exit/reentry, CCCD off/on, encrypted
bonded reconnect, MTU 23 fragmentation, 30 Hz complete delivery and Lights
latency under acquisition/radio load. Host tests and builds do not establish
actual radio delivery or acquisition timing.
