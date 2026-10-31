# Serial message reference for a phone UI

Source review: October 26, 2026. Describes the supplied firmware, whose version constants are `1.2-1.7` and build `26-06-29d`. This is a source-derived interface reference, not a hardware capture. No firmware changes are included.

## 1. Transport and stream

The firmware initializes `Serial` at **115200 baud**. For a physical UART, start with 8 data bits, no parity, one stop bit, and no flow control; those settings are not explicitly configured here. The actual phone connection depends on the board's USB/UART implementation and adapter. This firmware does not currently implement BLE transport.

Messages are ASCII text. Structured records end with CRLF (`\r\n`, bytes `0D 0A`). `Serial.println()` diagnostics share the same stream. There is no binary header, checksum, escaping, protocol negotiation, device identifier, sequence number, or end-of-snapshot marker.

The loop attempts output when more than **500 ms** has elapsed since the previous output; nominally about 2 updates/second, with delays possible. Every more than **5000 ms**, checked at that same output call, a heartbeat is written **before** the mode record(s). These are loop-driven intervals, not precise clocks. Battery voltage is refreshed separately every more than 10 seconds.

| Active firmware mode | Periodic output after optional heartbeat |
| --- | --- |
| `CABLE` | `C1`, then `C2`, normally with the same timestamp |
| `WPN_TEST` | `W` |
| `WPN_GRAPH` | `R` |
| `HIT_CAPTURE` | `R`; no captured sample data |
| `PROBE`, `BOX_IDLE`, `BOX_OFF` | No mode record; heartbeat can occur if the loop continues running |

Boot, calibration, settings, button, and mode-related diagnostics can occur between records. `C1` is specifically guarded by `if (Serial)` and flushed; the other records use the final unconditional write. Do not depend on every `C1` being followed by an available `C2`, or on both being delivered in a single read.

## 2. Shared field rules

* Commas separate fields in `C1`, `C2`, `W`, and `R`; labels and prefixes are case-sensitive.
* Timestamp `t` is a device `millis()` reading, in milliseconds since boot, printed using signed decimal `%ld`. It is not phone time or UTC. On these 32-bit targets, a long-running device can print negative timestamps after the signed boundary; unsigned millis wraps after roughly 49.7 days. Use modulo-32-bit arithmetic for elapsed time and reset session state on reconnect/reboot. A backward timestamp alone does not prove a reboot.
* Resistance fields use `dtostrf(value, 5, 2, ...)`: two decimal places and **minimum** width five, with leading spaces. Width is not a fixed field length: `999.90` has six characters. Parse by delimiter and trim whitespace.
* Resistances are in **ohms**, not tenths of an ohm. `999.90` represents the firmware's `999.9` open-circuit sentinel; render **OPEN** rather than treating it as a precise resistance. Negative calibrated readings can exist; preserve them for diagnostics rather than silently converting them to open circuit.
* The status field is an unprefixed lowercase hexadecimal integer, with no fixed width. For example `10` means hex `0x10`, not decimal ten. Despite the name `statusByte`, the storage is `uint16_t` and **nine bits** are used.
* A record's timestamp is taken while formatting the output, not at each channel's ADC acquisition. C1/C2 are associated snapshots, not proof that all nine channels were sampled simultaneously.

## 3. Structured records

Angle-bracket fields below are placeholders. Displayed `\r\n` denotes the actual two terminator bytes.

### C1: cable resistance and decaying peak

```text
C1,<t>,<status_hex>,A,<AA_ohm>,<AA_peak_ohm>,B,<BB_ohm>,<BB_peak_ohm>,C,<CC_ohm>,<CC_peak_ohm>\r\n
```

Exactly **12 comma-separated fields**. A/B/C identify AA/BB/CC through-connections. The first resistance is low-pass filtered; the second is a decaying maximum from the corresponding ADC channel, not a session-wide maximum or a mean. The ADC class uses a nominal exponential decay constant of 200 ms.

Illustrative record:

```text
C1,12500,10,A, 0.42, 0.80,B, 5.60, 6.10,C, 0.35, 0.55\r\n
```

This reports high resistance on BB (bit 4). Show three resistance tiles, three peak values, and faults decoded from the status word.

### C2: cable ADC matrix and optional display timing

```text
C2,<t>,<status_hex>,AA,<adc>,AB,<adc>,AC,<adc>,BA,<adc>,BB,<adc>,BC,<adc>,CA,<adc>,CB,<adc>,CC,<adc>[,DTime, <digits>l]\r\n
```

**21 fields**, or **23** with the optional `DTime` pair. Channel order is exactly AA, AB, AC, BA, BB, BC, CA, CB, CC. Values are decimal integers from `getRawValue()`, which actually returns the channel's decimated FIR-filtered ADC value. Nominal ADC resolution is 12 bits, 0–4095. These are ADC counts, not ohms; gain and trim information needed for reliable resistance conversion is absent from this record.

Illustrative record:

```text
C2,12500,10,AA,38,AB,4095,AC,4095,BA,4095,BB,502,BC,4095,CA,4095,CB,4095,CC,31,DTime, 2l\r\n
```

`DTime` appears only when `timeSamples > 0`. Its intended quantity is the accumulated average duration of cable/clip display drawing, measured in microseconds then divided by 1000 with integer truncation: whole **milliseconds per display update**. It is not ADC sampling time or a transport delay. The format string is incorrectly `%ul`, so the current output has a literal lowercase `l` suffix after a number (for example ` 2l`). The unsigned-long argument also mismatches `%u`; treat this field as an optional diagnostic until confirmed on hardware. A legacy parser can trim spaces and accept digits followed by one `l`; a malformed DTime should not discard otherwise valid channel readings.

### Cable status bit map (C1 and C2)

A set bit means a fault condition, not a good connection.

| Bit | Mask | Channel | Set condition in `updateCableState()` |
| --- | --- | --- | --- |
| 0 | `0x001` | AA | Decaying peak resistance > 5.0 ohms |
| 1 | `0x002` | AB | ADC < 1000, cross-connection/short |
| 2 | `0x004` | AC | ADC < 1000, cross-connection/short |
| 3 | `0x008` | BA | ADC < 1000, cross-connection/short |
| 4 | `0x010` | BB | Decaying peak resistance > 5.0 ohms |
| 5 | `0x020` | BC | ADC < 1000, cross-connection/short |
| 6 | `0x040` | CA | ADC < 1000, cross-connection/short |
| 7 | `0x080` | CB | ADC < 1000, cross-connection/short |
| 8 | `0x100` | CC | Decaying peak resistance > 5.0 ohms |

Diagonal open circuits also set the high-resistance bits because their sentinel exceeds 5 ohms. The status word does not distinguish an open from high resistance by itself; use C1 too. `100` is a valid status for CC fault; never truncate to eight bits. These are firmware thresholds, not a declaration of compliance with fencing equipment rules.

### W: digital weapon state

```text
W,<t>,E,<epee_on>,<last_epee_trigger_ms>,F,<foil_on>,<last_foil_trigger_ms>\r\n
```

Exactly **8 fields**. Boolean values are decimal `0` or `1`. They are the firmware's `epeeOn` and `foilOn` flags. In digital mode both are derived from inverted GPIO inputs; a value of 1 means that input is low. Verify the desired foil hit/open wording against actual wiring before labeling it as a scored hit.

Trigger fields are boot-relative timestamps of the last accepted pin-change interrupt, with debounce thresholds of more than 3 ms for epee and more than 15 ms for foil. Interrupts are configured for CHANGE, so these are **edge timestamps**, not exclusively press/hit events or pulse durations. Zero is the initial value. The flags are polled/debounced separately and need not describe precisely the edge at the transmitted timestamp.

```text
W,14000,E,1,13980,F,0,13200\r\n
```

At 2 Hz, transitions can occur entirely between records. A changed trigger timestamp indicates activity but cannot reconstruct every event, its polarity, or its duration. Intermittent/held-light flags are not transmitted.

### R: weapon resistance

```text
R,<t>,E,<epee_ohm>,F,<foil_ohm>\r\n
```

Exactly **6 fields**, for example:

```text
R,15000,E, 0.28,F,999.90\r\n
```

The values come from calibrated ADC resistance conversion, with the same open-circuit sentinel. The resistance update function assigns `getValue()` directly; do not assume these fields have the extra cable low-pass filter. `WPN_GRAPH` and `HIT_CAPTURE` produce identical R schemas, so the phone cannot distinguish them from the stream. R contains neither weapon state flags nor peak values nor capture duration/samples.

### Heartbeat: system diagnostics

```text
t=<t>, N=<sample_count>, <rate>Hz, Bat=<voltage>V\r\n
```

```text
t=15500, N=12000,  2400Hz, Bat= 3.85V\r\n
```

Four comma-separated segments, but a separate key/unit grammar from the telemetry records. `sample_count` is reset after each heartbeat; rate is that count divided by elapsed seconds since the previous heartbeat calculation, printed with width five and zero decimals. Voltage is width five with two decimals.

The counter is incremented on selected ADC channel visits (labels beginning A or E), or on loop iterations in digital weapon mode. It is mode-dependent and is **not** the serial update frequency, a universal per-channel sample rate, or sufficient metadata for a waveform time axis. Mode changes can mix the count's meaning within an interval. Show rate/count in diagnostics and battery voltage in the main status area. No percentage or charging state is transmitted.

## 4. Unstructured messages

Keep other text in a diagnostic log, not in the measurement parser. Examples of active output include:

| Origin | Examples / format |
| --- | --- |
| Startup | `Initializing channels`, `Cable data initialized`, `Weapon data initialized`, `Setup complete`, filesystem errors |
| Calibration load | `Read Calibration, got <n> channels`, `Cal Offset <label> = <n>`, missing/invalid calibration warnings |
| Calibration run/save | `Samples= <n> | <elapsed>us`, `Discarding`, `Saved`, `skipping`, `Write Calibration Data`, write success/error text |
| State/button | `Button pressed`, `Weapon connected, switching mode`, `Cable connection detected`, `Idle Power Off` |
| Settings | `Loading default settings...`, `Confirming setting: <n>`, persistence errors; settings reporting function formats `Settings Status:`, `Version: ...`, `Lame Display: ...`, `Weapon Display: ...`, `Advanced Mode: Enabled/Disabled` |
| Display | `Reset graph`, `Lame create-- Doing nothing at the moment`, `Undefined enum, try again` |

Some debug strings visible in source are inside comments and are not emitted: channel address dumps, menu-render traces, `Rave`, `Rmax`, `Xadc`, the initial `WF01...` format comment, and `SerialBT` output are not current protocol records. No serial input/command dispatcher was found: the existing port is a reporting interface, not a remote control interface.

## 5. Phone receiver behavior

1. Keep one receive buffer and model per connected physical device/port. Current records contain no device identity. A gateway combining ports must attach source identity before forwarding.
2. Accumulate arbitrary byte chunks; a read may contain half a record or several records. Split on LF, removing a preceding CR. A defensive 1024-byte line cap is reasonable; discard an overlong line through its next LF and log the error.
3. Match exact prefixes, then validate field counts, labels, finite decimal values, and hexadecimal status. Never treat malformed/unknown input as a zero reading or as a command.
4. Associate C1/C2 by device session and identical timestamp, with a short bounded cache (for example 2 seconds). Render C1 independently if C2 is missing; expire unmatched pieces. Do not combine different timestamps into a seemingly complete matrix snapshot.
5. Store both device time and phone arrival time. Use phone monotonic time for stale-data indicators. Suggested UI policy: mark measurements stale after 2 seconds without matching telemetry; heartbeat health stale after 12 seconds. Heartbeats alone do not make old measurements current. Missing mode telemetry can mean idle/probe, not necessarily disconnection.
6. Clear/hide measurements irrelevant to the new observed record family. Label R as weapon resistance with capture mode unknown. Preserve diagnostics separately. On reconnect, start a new session and show values as unavailable until refreshed.

The existing stream cannot fully mirror the local display: no explicit mode/state transitions, device identity/version record, probe values, lame-specific filtered values, mask/lame flags, full settings schema, waveform/capture metadata, event queue, ACKs, or remote commands. Startup status initialization also uses logical OR rather than a bitwise mask, so do not treat an initial status as established measurement truth before normal state updates.

## 6. Source locations and validation limits

* `DisplayHandling.ino:1300`: complete periodic formatter; `:658`: display duration measurement.
* `Feather_TestBox_v0.ino:178`, `:635`, `:858`: update interval, serial initialization, output scheduling; `:229`: bit definitions; `:370`: cable structure; `:702`: digital edge handlers.
* `ModesStates.ino:128`: cable values and fault logic; `:382`: weapon resistance; `:498`: digital flags.
* `ADC_Channel.cpp:78`: calibrated conversion and raw/peak getters; `Channel.h`: ADC depth; `Init_ADC.ino:1`: ADC setup.
* `eeprom.ino`, `Init_ADC.ino`, `MenuSystem.cpp`: diagnostic messages.

Formatting hazards to resolve in a future implementation: the formatter uses external `SERIAL_BUFFER_SIZE` while the actual array is allocated with `SERIAL_OUTPUT_BUFFER_SIZE=256`; these are not tied together in this repository. `strncat` receives a total buffer size rather than remaining space. Long/malformed output cannot be ruled out without inspecting the selected core and hardware behavior. The examples above are illustrative, not recorded device traffic. Validate real startup, each mode, faults, disconnects, and long-running timestamps before freezing a phone parser.
