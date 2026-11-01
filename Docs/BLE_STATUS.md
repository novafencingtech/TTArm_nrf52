# Status 2.1: app sync

November 1, 2026. Firmware build `26-11-01-timing` adds loop timing and negotiated
connection interval. Status UUID remains
`4a1e0020-3bb5-44b4-b566-8d22e72da6f5`. Standard encrypted Just Works pairing,
capabilities `0x00000201`, and all Cable bytes remain unchanged. Weapon, Probe,
Lights, Capture and commands remain unimplemented.

Read Public Protocol before decoding: **`54 54 42 4C 02 01 01 00`**. Full Identity
offsets 0..3 are now `02 01 01 44` (major 2, minor 1, layout 1, length 68).
The live header still contains only protocol major 2. Apps retaining support for
2.0 must explicitly select the old Status schema for minor 0 and the new schema
for minor 1. Do not silently decode a 2.1 frame using the 2.0 schema.

All integers are unsigned little-endian. Reassemble the six-byte transport
wrapper first, then validate the 14-byte live header and exact payload length.
Status is now **16-byte payload / 30-byte logical message / 36-byte wrapped
notification** when ATT MTU is at least 39, including MTU 247. At MTU 23, expect
three notification values of **20, 20, and 8 bytes**. Reassembly timeout remains
2 seconds; validate all fragments before updating app state.

| Payload offset | Logical offset | Single-notification offset | Type | Field / meaning |
| --- | --- | --- | --- | --- |
| 0 | 14 | 20 | u32 | Existing serial diagnostic interval count N |
| 4 | 18 | 24 | u32 | Existing ADC diagnostic rate, rounded whole Hz |
| 8 | 22 | 28 | u16 | Last valid measured battery millivolts |
| 10 | 24 | 30 | u32 | `average_loop_us`: average elapsed time between loop starts, microseconds |
| 14 | 28 | 34 | u16 | `connection_interval_units`: actual BLE interval, 1.25 ms units |

The first ten payload bytes retain their 2.0 meanings and offsets. Header payload
length at logical offset 10 is now 16; wrapper logical length at value offset 4
is now 30. Subscription/mode changes still request immediate Status (flag 2),
then periodic Status (flag 0) at approximately 1 Hz. Existing count/rate/battery
initialization gates remain in place.

Loop timing averages each completed window of at least one second, truncating
to whole microseconds; cached values repeat between windows. The first loop
start initializes timing and excludes setup. The mean includes display drawing,
acquisition processing, blocking serial output, BLE polling, delays, interrupts,
and early-return paths. It continues in menus and across reconnects/mode changes.
Zero means waiting for the first completed window. For display, use
`average_loop_us / 1000.0` milliseconds. `1_000_000.0 / average_loop_us` estimates
average loop Hz when nonzero; neither value measures Cable delivery rate or
maximum poll gap.

BLE interval comes from stock `BLEConnection::getConnectionInterval()`, including
accepted connection parameter updates. Convert to milliseconds with
`connection_interval_units * 1.25`. Values 6..3200 are valid; zero means unknown.
For example, 12 = 15 ms, 24 = 30 ms, 40 = 50 ms. Active modes request
12 units only (15 ms); Idle requests 144..160 units (180..200 ms).
The phone negotiates the actual value. MTU 247
does not imply any particular interval or notification rate. Both new fields
are frozen when a frame is formed, including during retries.

## Mode-based interval requests

Firmware requests the current mode's range after encryption on each connection
and immediately on an actual mode change. Cable, Weapon test, Weapon graph,
local Hit Capture, and Probe request exactly 15 ms (minimum = maximum = 12 units).
Idle requests 180..200 ms.
Peripheral latency stays zero and supervision timeout stays 4 seconds.

During active modes, the foreground BLE poll checks the actual negotiated
interval. If it is greater than 30 ms, it retries the faster range every five
seconds until the interval is at most 30 ms. If the phone later increases it,
requests resume. Busy/rejected local request attempts are also spaced five
seconds apart; there are no waits or catch-up bursts. Mode changes and reconnects
start a fresh immediate attempt, deferred until encrypted if necessary. Once an
Idle request is submitted successfully, no periodic Idle renegotiation is forced.
Off, disconnect, shutdown, and bond removal suppress requests.

This uses Bluefruit's preferred-parameter API and the stock SoftDevice update
call, which supports the requested range. The phone can decline the request;
Status continues to report the actual accepted interval. No app command or wire
layout change is needed beyond the existing 2.1 Status decoder.

## Synthetic shared byte vector

Mode Cable (0), immediate flag 2, sequence 0, uptime 1000 ms, epoch 1,
N = 10000, ADC rate = 2000 Hz, battery = 3700 mV, average loop = 12500 us
(12.5 ms), connection interval = 24 units (30 ms), transport ID 0.

Logical message (30 bytes):

```hex
02 01 02 00 00 00 E8 03 00 00 10 00 01 00 10 27 00 00 D0 07 00 00 74 0E D4 30 00 00 18 00
```

One notification at MTU 247 (36 bytes):

```hex
00 00 00 01 1E 00 02 01 02 00 00 00 E8 03 00 00 10 00 01 00 10 27 00 00 D0 07 00 00 74 0E D4 30 00 00 18 00
```

Three notifications at MTU 23 (20/20/8 bytes):

```hex
00 00 00 03 1E 00 02 01 02 00 00 00 E8 03 00 00 10 00 01 00
00 00 01 03 1E 00 10 27 00 00 D0 07 00 00 74 0E D4 30 00 00
00 00 02 03 1E 00 18 00
```

These are synthetic codec examples. The timing additions and interval policy require hardware
verification; the earlier Status and Cable implementations were hardware verified.
Only basic board compilation checks are requested for this change.

Basic stock Arduino compilation passed on November 1, 2026 for nRF52832 and
nRF52840 (Adafruit nRF52 core 1.3.0), without compiler overrides, including the
mode-based interval policy. nRF52832:
182,940 flash / 29,844 static RAM bytes. nRF52840: 232,036 flash / 35,008 static
RAM bytes. Existing CMSIS/Arduino PI and FastLED board warnings remain. Portable
fixture expectations were updated for 2.1; the test suite was not run. No hardware
was flashed.

Subsequent hardware testing isolated the Cable delivery slowdown to OLED label
redraws: caching unchanged rendered text/color restored the observed update rate.
Labels are invalidated after mode/layout clears so they redraw once, then resume
caching. The user confirmed both the latency fix and label restoration on
November 1, 2026. Final basic compilation with these OLED changes passed for
nRF52832 (183,348 flash / 30,044 static RAM bytes) and nRF52840 (232,460 flash /
35,208 static RAM bytes).
