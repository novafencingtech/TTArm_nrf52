# TournaTest BLE Message Specification

**Protocol: 2.0 · Specification revision: 1 · October 30, 2026**

This is the common implementation contract for Nordic firmware and Android/iOS receivers. Implementers SHALL use the UUIDs, bytes and behavior in this document. It takes precedence over the [implementation plan](BLE_IMPLEMENTATION_PLAN.md) and historical drafts for wire-format questions. It is an agreed-design specification for implementation, not a claim of existing compatible firmware or hardware qualification. Protocol 1 draft codecs are incompatible.

MUST/SHALL describe requirements; SHOULD describes a recommendation. An offset is zero-based. Lengths are bytes. The protocol version is independent of the app version, firmware version and Bluetooth Core version.

Contents: [GATT](#2-gatt-database), [identity](#4-identity-values), [framing](#5-live-message-header), [fragmentation](#6-notification-fragmentation), [messages](#7-live-message-payloads), [commands](#8-control-commands), [settings](#9-settings-registry), [session](#10-session-and-device-behavior), [examples](#11-shared-byte-examples), [conformance](#12-conformance-checklist).

## 1. Scope and encoding

- The tester is the BLE peripheral/GATT server; the phone is the central/client. One phone connects to one box at a time; a box accepts one connected phone at a time.
- Cable resistance/fault and ADC data are **one combined message at 30 Hz**. Weapon and Probe are 30 Hz. Lights follows accepted hardware-handler changes, with a full-state refresh at 1 Hz. Status is 1 Hz.
- Published measurement fields are valid. There are **no measurement-validity masks**. Firmware SHALL withhold a message until all of its fields are initialized and applicable. A receiver SHALL still validate encoding and reject malformed messages.
- Lame is a **display setting/presentation**, not an acquisition mode or separate characteristic. Advanced Mode's cycle-order behavior is not a BLE setting in this specification.
- The user controls are SetBoxMode, settings read/write, and manual calibration. Capture waveform transfer and retained event history are outside protocol 2.0; their capability bits remain zero.

### 1.1 Primitive types and units

| Type | Encoding |
| --- | --- |
| `u8` | Unsigned 8-bit integer |
| `u16` | Unsigned 16-bit integer, little-endian |
| `u32` | Unsigned 32-bit integer, little-endian |
| `u64` | Unsigned 64-bit integer, little-endian |
| `i32` | Signed 32-bit two's-complement integer, little-endian |
| Fixed text | UTF-8 followed by a zero terminator and zero padding to the field length |

Encode/decode fields explicitly. C/C++ native structure layout and Kotlin/Swift object layout SHALL NOT determine wire bytes. Fields may be unaligned. No floats, implicit padding, JSON or debug strings are carried in custom messages. Reserved fields/bits SHALL be zero; receivers reject nonzero reserved fields in 2.0.

Resistance is **milliohms**: signed integer `420` represents 0.420 Ω. `0x7FFFFFFF` represents electrical **OPEN**, which is a valid result. Other finite resistance values range from `-2147483647` through `2147483646`; preserve signed calibrated values. `0x80000000` is reserved and SHALL NOT be transmitted as an unavailable-reading placeholder. A received live message containing it is malformed.

Cable ADC values are `u16` counts in 0–4095. The value 4095 is valid; `0xFFFF` is not a live ADC reading. Battery is `u16` millivolts, 0–65534; `0xFFFF` is reserved. Firmware SHALL send actual valid measured values without inventing a percentage, charging flag or acquisition counter.

## 2. GATT database

### 2.1 Custom service and characteristic UUIDs

All boxes implementing 2.0 use the same UUIDs. UUIDs identify the protocol, not an individual physical box. Handles are discovered and SHALL NOT be hard-coded.

| Role | UUID | Properties | Characteristic maximum value length |
| --- | --- | --- | --- |
| Tester service | `4a1e0000-3bb5-44b4-b566-8d22e72da6f5` | Primary service | — |
| Public Protocol | `4a1e0001-3bb5-44b4-b566-8d22e72da6f5` | Read | 8 |
| Full Identity | `4a1e0002-3bb5-44b4-b566-8d22e72da6f5` | Read | 68 |
| Cable | `4a1e0010-3bb5-44b4-b566-8d22e72da6f5` | Notify | 66 |
| Weapon | `4a1e0011-3bb5-44b4-b566-8d22e72da6f5` | Notify | 66 |
| Capture, reserved | `4a1e0012-3bb5-44b4-b566-8d22e72da6f5` | Notify | 66 |
| Probe | `4a1e0013-3bb5-44b4-b566-8d22e72da6f5` | Notify | 66 |
| Lights | `4a1e0014-3bb5-44b4-b566-8d22e72da6f5` | Notify | 66 |
| Status | `4a1e0020-3bb5-44b4-b566-8d22e72da6f5` | Notify | 66 |
| Control | `4a1e0030-3bb5-44b4-b566-8d22e72da6f5` | Write with response | 9 |
| Command Result | `4a1e0031-3bb5-44b4-b566-8d22e72da6f5` | Indicate | 10 |

Characteristic values SHALL be variable length up to the listed limit. Do not pad a 30-byte Status or 32-byte Lights value to 66 bytes. Register the reserved Capture characteristic for database stability, but send no Capture notifications in 2.0. Register all listed characteristics even if an optional capability is unavailable.

Each Notify/Indicate characteristic has its own standard CCCD, UUID `00002902-0000-1000-8000-00805f9b34fb`:

| Operation | CCCD bytes |
| --- | --- |
| Enable notification | `01 00` |
| Enable indication | `02 00` |
| Disable | `00 00` |

No custom subscription command exists. Enabled CCCDs, authenticated security and active mode determine publication. A disabled CCCD prevents delivery on its characteristic. Multiple mode subscriptions are allowed, but do not cause simultaneous acquisition modes.

### 2.2 Standard services and access

Provide stack-managed Generic Access (`0x1800`) and Generic Attribute (`0x1801`). Use Service Changed (`0x2A05`) when firmware can change the GATT database, following the stack's bonded-client rules. Device Information (`0x180A`) may expose manufacturer, model and firmware/hardware revision strings with their standard characteristic UUIDs. Battery Service is not required; Status carries voltage.

Public Protocol and nonidentifying product strings are publicly readable. Full Identity, Control writes and all measurement/result delivery require **LE Security Mode 1 Level 4**, authenticated LE Secure Connections with 128-bit-key encryption. CCCD reads are public; CCCD writes require authenticated encryption. A saved bond alone does not establish that the current connection is encrypted/authenticated. Refuse weaker protected access and pairing downgrade.

## 3. Discovery, identity and connection parameters

### 3.1 Advertising

Use legacy connectable advertising, LE1M, with the service UUID above. Recommended advertising data is 21 bytes: Flags `02 01 06`, followed by a complete 128-bit service UUID list (`11 07` plus 16 UUID octets). The complete service advertising bytes are:

```hex
02 01 06 11 07 F5 A6 2D E7 22 8D 66 B5 B4 44 B5 3B 00 00 1E 4A
```

Use the SDK UUID encoder; BLE UUID wire order is separate from payload scalar encoding. Put a name such as `TTArm-A1B2C3` in scan response: 12 UTF-8 bytes plus two AD overhead bytes. The suffix is the device ID's low 24 bits and is only a discovery hint. Do not identify the box by name, RSSI or advertised MAC.

Recommended configured intervals are 20 ms for the first 30 s of a local pairing window, 152.5 ms for the remainder of its 60 s, then 546.25 ms for known-device discovery. Stop connectable advertising while the one connection slot is occupied. A physical local action authorizes new pairing; show a fresh six-digit passkey on the box for entry on the phone. Saved bonds restore authenticated encryption without reentering that code. Bond removal/recovery remains a local box action.

### 3.2 Physical device and boot identity

`device_id` is a stable, nonzero 64-bit application identity. For an nRF52 default, use `(u64(DEVICEID[1]) << 32) | DEVICEID[0]`, checked for duplicates during provisioning, or a provisioned override. Its printed form is 16 hexadecimal digits, most significant digit first. Settings resets and firmware upgrades SHALL preserve it. Chip replacement changes a hardware-derived ID unless an explicit repair/provisioning process preserves box identity.

`boot_id` is a fresh nonzero random 64-bit value each firmware boot, unchanged through reconnects in that boot. Receivers key state by full device ID + boot ID + phone-local connection generation. Old connection callbacks SHALL NOT enter the new session. A Bluetooth identity address can be static random, and privacy may rotate the advertised address; the BLE stack manages those addresses and keys.

### 3.3 MTU and radio configuration

Recommended starting configuration is **ATT MTU ceiling247**, **DLE up to251 data octets**, LE1M support, interval request **15–30 ms**, peripheral latency0, supervision timeout4 s. The central determines actual accepted parameters. Android requests a larger MTU and uses `onMtuChanged`; iOS manages MTU negotiation. Never assume the requested size was granted.

The current maximum streamed value is66 bytes. It needs ATT MTU≥69 for one notification. Keep that value length even when ATT MTU is185 or247; do not pad it. Its ATT/L2CAP data occupy 66+3+4=73 octets. It can fit one extended link-layer packet when the negotiated TX data length/time permits; without DLE the stack may split it into link-layer packets even though it is one ATT notification. ATT fragmentation in section6 is a separate application mechanism for smaller ATT MTUs. Default MTU23 SHALL work.

## 4. Identity values

These are direct GATT Read values. They have **no live-message header and no fragment wrapper**. Long Identity reads use normal GATT Read/Read Blob; a client SHALL obtain all 68 bytes and validate them before accepting live data. Identity remains immutable throughout a connection, including a multistep long read.

### 4.1 Public Protocol — exactly 8 bytes

| Offset | Type/length | Field | Required value |
| --- | --- | --- | --- |
| 0 | bytes[4] | magic | ASCII `TTBL`, hex `54 54 42 4C` |
| 4 | u8 | protocol_major | 2 |
| 5 | u8 | protocol_minor | 0 |
| 6 | u8 | identity_layout | 1 |
| 7 | u8 | reserved | 0 |

For 2.0, the client SHALL require exactly these bytes: `54 54 42 4C 02 00 01 00`. A different major/minor/layout is unsupported; do not guess its schema.

### 4.2 Full Identity — exactly 68 bytes

| Offset | Type/length | Field | Definition |
| --- | --- | --- | --- |
| 0 | u8 | protocol_major | 2 |
| 1 | u8 | protocol_minor | 0 |
| 2 | u8 | identity_layout | 1 |
| 3 | u8 | identity_length | 68 |
| 4 | u64 | device_id | Nonzero stable box ID |
| 12 | u64 | boot_id | Nonzero random per boot |
| 20 | u32 | capabilities | Section4.3 |
| 24 | u16 | model_id | 1 = TTArm |
| 26 | u16 | hardware_revision | Provisioned revision; 0 unspecified |
| 28 | u8 | firmware_major | Firmware version component |
| 29 | u8 | firmware_minor | Firmware version component |
| 30 | u8 | firmware_patch | Firmware version component |
| 31 | u8 | reserved0 | 0 |
| 32 | bytes[24] | firmware_build | UTF-8, at most23 nonzero bytes, terminator and zero padding |
| 56 | u16 | maximum_logical_length | 512 |
| 58 | u8 | maximum_characteristic_value | 66, storage/protocol ceiling, **not negotiated MTU** |
| 59 | u8 | measurement_rate_Hz | 30 |
| 60 | u16 | reserved1 | 0, event history deferred |
| 62 | u16 | reserved2 | 0, capture transfer deferred |
| 64 | u8 | required_security_level | 4 |
| 65 | u8 | identity_source | 0 factory ID, 1 provisioned override |
| 66 | u16 | reserved3 | 0 |

Model ID, layout and fixed limit fields SHALL have the specified values. An unknown hardware revision/build is not authority to reinterpret the payload. Reject invalid UTF-8 or nonzero bytes following the build string's first zero. Firmware version fields identify firmware, not protocol compatibility.

### 4.3 Capability bits

| Bit | Mask | Meaning |
| --- | --- | --- |
| 0 | `0x00000001` | Complete combined Cable stream |
| 1 | `0x00000002` | Complete Weapon stream, including all flags |
| 2 | `0x00000004` | Complete Probe stream |
| 3 | `0x00000008` | Complete Lights stream, including both latch indicators |
| 7 | `0x00000080` | SET_BOX_MODE |
| 8 | `0x00000100` | GET_SETTING and SET_SETTING for a registered settings profile |
| 9 | `0x00000200` | Battery millivolt acquisition |
| 10 | `0x00000400` | MANUAL_CALIBRATE |

Bits4–6 and11–31 are reserved zero; allowed mask is **`0x0000078F`**. Reserved history/Capture bits SHALL NOT be used for the existing Lights event handler or local Capture mode. A set stream bit promises all fields of that stream; do not use partial placeholder messages. A clear command capability returns UNSUPPORTED for that operation. Status is the fixed complete diagnostics message in section7.5 and is published only when its count/rate/voltage have been initialized. An implementation without usable battery diagnostics cannot publish this complete Status; its battery bit stays clear and the client shows diagnostics unsupported rather than fabricated values.

## 5. Live-message header

Cable, Weapon, Probe, Lights and Status use a **14-byte header** followed by exactly the stated payload. Control and Command Result use section8 directly and SHALL NOT receive this header.

| Offset | Type | Field | Definition |
| --- | --- | --- | --- |
| 0 | u8 | protocol_major | 2 |
| 1 | u8 | message_type | Section5.1 |
| 2 | u8 | flags | Only bit 1 (`0x02`) is defined: immediate state/subscription update |
| 3 | u8 | mode | Section5.2 |
| 4 | u16 | sequence | Connection-local, per output characteristic, modulo65536 |
| 6 | u32 | uptime_ms | Device monotonic publication time, modulo2^32 |
| 10 | u16 | payload_length | Exact number of following payload bytes |
| 12 | u16 | mode_epoch | Boot-local acquisition-mode generation, modulo65536 |

The maximum logical message is512 bytes, including this header. No CRC follows ordinary connected messages. Link security/integrity and schema validation are used; no custom ASCII terminators are present.

### 5.1 Message registry and exact lengths

| Characteristic | Type | Payload bytes | Logical bytes | One-fragment value bytes | Active modes |
| --- | --- | --- | --- | --- | --- |
| Cable | `0x10` | 46 | 60 | 66 | 0 |
| Weapon | `0x12` | 19 | 33 | 39 | 2,3 |
| Probe | `0x13` | 26 | 40 | 46 | 4 |
| Lights | `0x14` | 12 | 26 | 32 | 1 |
| Status | `0x01` | 10 | 24 | 30 | 0–6 while radio is powered |

The type SHALL match its characteristic, payload length and mode. Type`0x11` is retired; do not send separate Cable ADC. Types`0x20`–`0x23`, `0x30`/`0x31` stream envelopes, and Capture types`0x40`–`0x43` are not part of this profile. A receiver rejects an unsupported/mismatched type without updating measurements or command state.

### 5.2 Modes

| Value | Acquisition mode | Phone presentation |
| --- | --- | --- |
| 0 | Cable | Cord or Lame display using the Cable data |
| 1 | Digital weapon | Lights |
| 2 | Weapon resistance | Weapon |
| 3 | Local hit capture | Live Weapon cards; waveform transfer unavailable in 2.0 |
| 4 | Probe | Probe |
| 5 | Idle | No mode-measurement publications; Status may continue |
| 6 | Off | Only a best-effort pre-shutdown Status; no radio communication after power-off |

Values7–255 are invalid in live headers. Lame and Advanced Mode do not receive mode numbers. A local button or automatic mode transition and SET_BOX_MODE use the same mode owner; local behavior retains its normal precedence.

### 5.3 Counters and timestamps

`sequence` starts at 0 for each characteristic on each connection and increments for each newly formed logical transmission, including unchanged refresh/subscription copies. Every fragment of a message carries the same logical header. Discarding a pending message may produce a sequence gap. Sequence does not count ADC acquisitions or scheduled deadlines.

`mode_epoch` starts at 0 per boot and increments once for each actual acquisition-mode transition. Selecting the current mode does not increment it. On a newer epoch, the client clears incompatible live data immediately and rejects earlier epochs, including an earlier visit to the same mode. Same-epoch/different-mode is malformed. A missing intermediate epoch is possible; current Status resolves mode state. Counters are scoped by verified device/boot/connection identities.

For a 16-bit counter, `delta=(new-old)&0xFFFF`: delta0 is equal; 1–32767 is newer; 32768 is ambiguous; 32769–65535 is older. Never use comparison across a half-range outage to infer ordered history. Start a fresh observation baseline after reconnect or an ambiguous gap. A first valid message on a connection establishes the observed epoch baseline; it need not be0 because the box may have changed modes before connection.

`uptime_ms` is not UTC or phone time. Use modulo32-bit elapsed arithmetic within a half-range window. It timestamps logical message creation: periodic measurement publication, accepted Lights handler state capture, or a new state refresh. It does not imply simultaneous ADC sampling. A timestamp jump alone does not prove reboot; the boot ID does.

Every measurement payload starts with a boot-local `snapshot_id:u16`. Start its counter at 0 and increment when forming a new periodic publication or accepting a Lights state change for publication. The first newly formed snapshot has ID1. ADC shares Cable's ID. Initial/subscription copies and Lights 1 Hz refresh reuse the current state ID; a copy before any newly formed snapshot may have ID0. Snapshot IDs do not reset on mode changes, CCCD changes or reconnects within a boot. Use mode_epoch and timestamps as well as IDs; these are association/drop-diagnostic identifiers, not an acquisition counter or retained event journal.

Header flag bit 1 is set on immediate subscription, mode-entry or accepted Lights-change snapshots and clear on scheduled periodic measurements/refreshes. Reserved bits remain zero. Status sent on subscription/mode change uses bit 1; periodic Status uses0. A receiver SHALL accept an unchanged Lights snapshot ID with a newer transmission sequence as a current-state refresh.

## 6. Notification fragmentation

### 6.1 Wrapper — exactly 6 bytes before every logical slice

This wrapper is present on **every live/Status notification**, including a one-fragment message. It is not present on Identity reads, Control writes or Result indications.

| Offset | Type | Field | Definition |
| --- | --- | --- | --- |
| 0 | u16 | transport_message_id | Connection-local per characteristic, modulo65536 |
| 2 | u8 | fragment_index | Zero-based |
| 3 | u8 | fragment_count | Number of fragments, at least1 |
| 4 | u16 | total_logical_length | Header + payload bytes, 14–512 |
| 6 | bytes | slice | Consecutive bytes from the logical message |

The transport ID starts0 per characteristic/connection and advances once per new logical transmission. It is independent of header sequence and need not numerically equal it. Fragments from different messages SHALL NOT interleave on one characteristic. Characteristics have independent reassembly state.

### 6.2 Sender algorithm

At the beginning of a message, choose `V=min(66,actual_ATT_MTU-3)` and `S=V-6`. For normal ATT MTU≥23, V is20–66 and S is14–60.

1. Form one immutable logical message, length L.
2. Calculate `N=ceil(L/S)` and use that count and total length in every wrapper.
3. Send slices `[0:S]`, `[S:2S]`, etc., with indices0 through N−1.
4. Every nonfinal slice has exactly S bytes; the final slice has the remaining bytes. Do not pad it.
5. Keep S constant throughout this message. Apply an MTU increase only to a new message.

There is no HELLO, fragment-size negotiation or stream-start command. If the effective capacity unexpectedly becomes too small for a partly sent message, abandon it and reestablish the session rather than changing its slice boundaries. BLE backpressure SHALL NOT block measurement/interrupt handlers.

### 6.3 Receiver algorithm

Bound each notification value to7–66 bytes before copying. Validate total length 14–512, positive count and index<count. Only fragment0 may allocate/start a reassembly buffer.

For fragment0 with count>1, infer `S=value_length-6`, require 14≤S≤60, and verify `count=ceil(total_length/S)`. For count1, require index0 and exactly `value_length-6=total_length`. Do not infer S from an iOS write-limit API or assume MTU247.

For remaining fragments require matching ID, count and length, exact next index, exact S-byte nonfinal slices and exact final remainder. No gaps, duplicates, overlaps or out-of-order fragments may be used to complete a message. A newer fragment0 replaces an incomplete message; old already-completed IDs are ignored within the modular comparison window. A duplicate fragment or inconsistent active message invalidates that incomplete message. A nonzero fragment without matching active reassembly is ignored. Never splice bytes from different IDs.

Allocate at most512 bytes per output characteristic. Expire reassembly **100 ms from first fragment for Cable/Weapon/Probe/Lights** and **2 s for Status**; later fragments do not extend that deadline. Clear on disconnect, boot change or an accepted newer mode epoch. After complete reassembly, validate header/type/mode/length/reserved fields and every payload field before publishing any state. Partial data SHALL NOT refresh the display.

### 6.4 Cable example sizes

| ATT MTU | V | Combined Cable fragments | Total characteristic-value bytes per snapshot |
| --- | --- | --- | --- |
| 23 | 20 | 5 | 90 |
| 53 | 50 | 2 | 72 |
| 67 | 64 | 2 | 72 |
| 69 | 66 | 1 | 66 |
| 185 | 66 | 1 | 66 |
| 247 | 66 | 1 | 66 |

The maximum permitted logical length 512 would need37 fragments at V20 or9 at V66, but no current baseline payload needs that size.

## 7. Live-message payloads

The offsets below begin **after** the 14-byte logical header. All fields are required; no validity byte/mask is appended.

### 7.1 Cable (`0x10`) — 46 bytes

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u16 | snapshot_id |
| 2 | u16 | fault_mask |
| 4 | i32 | AA_mOhm |
| 8 | i32 | AA_peak_mOhm |
| 12 | i32 | BB_mOhm |
| 16 | i32 | BB_peak_mOhm |
| 20 | i32 | CC_mOhm |
| 24 | i32 | CC_peak_mOhm |
| 28 | u16 | ADC_AA |
| 30 | u16 | ADC_AB |
| 32 | u16 | ADC_AC |
| 34 | u16 | ADC_BA |
| 36 | u16 | ADC_BB |
| 38 | u16 | ADC_BC |
| 40 | u16 | ADC_CA |
| 42 | u16 | ADC_CB |
| 44 | u16 | ADC_CC |

Copy all fields coherently from a completed Cable measurement state. Through resistances are the existing filtered values; peaks are the existing decaying peaks, not lifetime maxima. ADC fields follow the firmware's existing matrix/FIR/decimated path and are not ohms. Copy coherence does not claim simultaneous acquisition of nine ADC channels. This combines the contents of serial C1 and C2; the serial interface may continue to send those as separate records at the same30 Hz cadence.

The fault mask uses only bits 0–8. A set bit means a fault/connected-cross condition:

| Bit | Mask | Path | Existing firmware interpretation |
| --- | --- | --- | --- |
| 0 | `0x001` | AA | High/open through resistance |
| 1 | `0x002` | AB | Directed cross connection |
| 2 | `0x004` | AC | Directed cross connection |
| 3 | `0x008` | BA | Directed cross connection |
| 4 | `0x010` | BB | High/open through resistance |
| 5 | `0x020` | BC | Directed cross connection |
| 6 | `0x040` | CA | Directed cross connection |
| 7 | `0x080` | CB | Directed cross connection |
| 8 | `0x100` | CC | High/open through resistance |

Bits9–15 are zero. The six-cross mask is`0x0EE`; bit 8 is CC, not a cross. Legacy thresholds are diagonal peak>5 Ω and off-diagonal ADC<1000. Use the box's actual fault computation; phone colors/classification do not redefine it. Do not publish a startup mask before normal initialization has completed.

### 7.2 Weapon (`0x12`) — 19 bytes

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u16 | snapshot_id |
| 2 | u8 | state_flags |
| 3 | i32 | E_mOhm |
| 7 | i32 | F_mOhm |
| 11 | u32 | last_epee_edge_ms |
| 15 | u32 | last_foil_edge_ms |

`state_flags`: bit 0 E active, bit 1 F active, bit 2 E intermittent/latch, bit 3 F intermittent/latch, bit 4 AC/ground short; bits 5–7 zero. Physical input-active semantics follow the existing firmware and do not themselves declare a legal fencing score. A zero last-edge value is the initial no-recorded-edge state; timestamps wrap naturally and SHALL NOT alone prove event existence or reboot. Resistance and flags are independent evidence.

Publish in modes2 and3. Mode3 values remain **live** and do not represent a captured waveform. Short transitions between periodic samples are not a complete event log.

### 7.3 Probe (`0x13`) — 26 bytes

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u16 | snapshot_id |
| 2 | i32 | E_mOhm |
| 6 | i32 | F_mOhm |
| 10 | i32 | WpnAC_mOhm |
| 14 | i32 | APr_mOhm |
| 18 | i32 | BPr_mOhm |
| 22 | i32 | CPr_mOhm |

The three Probe gauges use APr/BPr/CPr. E/F/G electrical availability uses finite E/F/WpnAC respectively, with OPEN meaning disconnected. Do not reuse previous Weapon-mode fields or infer a contact state from a different Probe channel.

### 7.4 Lights (`0x14`) — 12 bytes

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u16 | snapshot_id |
| 2 | u8 | light_flags |
| 3 | u8 | reserved |
| 4 | u32 | last_epee_edge_ms |
| 8 | u32 | last_foil_edge_ms |

`light_flags`: bit 0 E active, bit 1 F active, bit 2 E intermittent/latch, bit 3 F intermittent/latch; bits 4–7 zero. Reserved byte3 is zero. All four indicators SHALL mirror the hardware handler's accepted state, including its existing latch hold/clear behavior. Zero last-edge timestamps have the same initialization/wrap interpretation as Weapon.

When an accepted hardware event changes any transmitted state or edge timestamp, copy the full payload and its mode epoch/time into a bounded FIFO and notify from the BLE worker as soon as transmit credits permit. If the handler runs in an interrupt, capture/enqueue there without blocking; use only stack calls allowed at that priority. The initial queue target is16 pending states with a maximum target unsent age100 ms. Preserve accepted-change order normally; on overflow discard old pending states and retain the newest complete state. Do not claim lossless delivery or build delayed UI playback. Snapshot ID gaps expose discarded formed states.

Send a full state immediately when subscribed or entering Lights mode, then **on changes**, plus a **1 Hz unchanged full-state refresh**. Refresh reuses the latest snapshot ID/edge times but has a new header sequence, publication uptime and transport ID. It is not another electrical event. Firmware may merge a due refresh with a change notification that already publishes the newest state at that instant. Every value is a full state; never send a toggle-only command.

The phone may receive activation and release before drawing a frame. Preserve actual hardware latch flags to display brief activity; do not silently synthesize a persistent latch or claim every sub-frame pulse visibly flashes. Full-state refresh repairs a lost final state notification. Retained edge history/replay is outside this profile.

### 7.5 Status (`0x01`) — 10 bytes

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u32 | diagnostic_count |
| 4 | u32 | diagnostic_rate_Hz |
| 8 | u16 | battery_mV |

This matches the existing serial diagnostics `t=<uptime>, N=<count>, <rate>Hz, Bat=<voltage>V`, with uptime already in the logical header and voltage converted to millivolts. `diagnostic_count` is the existing interval N, not an added per-channel/lifetime acquisition counter. `diagnostic_rate_Hz` is the existing rate rounded to a whole Hz, not BLE delivery rate or waveform timing.

Reuse a coherent cached copy of the existing diagnostic calculation. BLE publication SHALL NOT reset N, change the serial diagnostic calculation schedule, or recalculate rate with a new interval. The reference updates diagnostics about every 5 s and battery about every 10 s; 1 Hz Status repeats the last valid calculated values between updates. Header uptime timestamps the new Status message, not new battery acquisition. Do not add new acquisition/delivery counters, validity flags, extended state flags or settings to this payload.

Publish only once all three diagnostics are initialized, on subscription/mode change and at 1 Hz thereafter. Before first initialized Status the phone shows diagnostics waiting, not fabricated values. Status does not refresh an old measurement or assert that a quiet Lights state has been refreshed.

## 8. Control commands

Control and Command Result values are direct bytes, **without** section5's14-byte header or section6's6-byte wrapper. All operations fit default ATT MTU23. The phone SHALL enable result indications before writing Control. The server SHALL reject a Control write at ATT without executing it if result indications are disabled or security is insufficient.

### 8.1 Control header — 3 bytes

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u8 | opcode |
| 1 | u16 | request_id |
| 3 | bytes | arguments |

Request ID is1–65535, strictly increasing per connection. Firmware maintains a connection-local high-water ID, initially0. A parseable nonzero ID above the high-water value is consumed before argument/capability validation; a failing command still consumes its ID. ID0 or an ID≤high-water returns INVALID_ARGUMENT without execution. Do not wrap IDs in a connection; reconnect before reuse. IDs are independent of live sequences/snapshot IDs.

Only one command SHALL be pending on the phone, including an in-progress calibration. Firmware may return BUSY for a new command while a local or remote operation conflicts. Preserve the original calibration request identity until its final result; receiving a rejected newer request must not relabel that result. No retry cache or automatic state-changing replay exists.

An ATT Write Response acknowledges that the write was accepted. **Only Command Result indicates execution outcome.** A write shorter than 3 or longer than 9 bytes is rejected at ATT without execution; no parseable application result is required. A parseable command with wrong opcode-specific length returns INVALID_ARGUMENT. Unknown opcodes or unsupported capabilities return UNSUPPORTED. Malformed arguments SHALL NOT partially change state.

### 8.2 Opcode registry and exact lengths

| Opcode | Name | Arguments | Control value length |
| --- | --- | --- | --- |
| `0x01` | SET_BOX_MODE | `mode:u8 @3` | 4 |
| `0x02` | GET_SETTING | `setting_id:u16 @3` | 5 |
| `0x03` | SET_SETTING | `setting_id:u16 @3, value:i32 @5` | 9 |
| `0x04` | MANUAL_CALIBRATE | None | 3 |

Other opcodes are unassigned. HELLO, stream-start, event ACK/replay, calibration cancellation and capture-transfer commands are not defined by 2.0.

### 8.3 Command Result header — 4 bytes

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u8 | opcode |
| 1 | u16 | request_id |
| 3 | u8 | status |
| 4 | bytes | detail, present only for successful GET_SETTING |

The opcode and ID echo the request, including application errors. Values are exactly 4 bytes, except a successful GET_SETTING which is exactly 10 bytes:

| Offset | Type | Successful GET_SETTING detail |
| --- | --- | --- |
| 4 | u16 | setting_id, matching request |
| 6 | i32 | current value |

No result contains prose, a reason-offset structure or padding. A receiver rejects wrong length, unknown status, mismatched setting ID or an unmatched result. A late result may be logged but must not complete another request.

| Status | Name | Meaning |
| --- | --- | --- |
| `0x00` | SUCCESS | Operation completed successfully |
| `0x01` | INVALID_ARGUMENT | Bad ID, argument, exact length, range or mode value |
| `0x02` | UNSUPPORTED | Opcode, capability, registered setting or requested local mode unavailable |
| `0x03` | BUSY | Conflicting operation; no requested change applied |
| `0x04` | DENIED | Firmware's existing local/preparation/access policy refused the action |
| `0x05` | FAILED | Execution failed; app cannot assume the requested outcome/persistence |
| `0x06` | IN_PROGRESS | Calibration accepted and running; final result follows |

Codes7–255 are invalid. IN_PROGRESS is used only for MANUAL_CALIBRATE. Every other status is final. GATT indications are serialized one at a time by the BLE stack; no application ACK command follows them.

### 8.4 SetBoxMode

Modes0–5 are valid arguments;6 Off and7–255 return INVALID_ARGUMENT. A valid numbered mode unsupported by this firmware returns UNSUPPORTED. Capability bit 7 is required. Lame is not a mode argument. Mode3 means enter the local capture mode, not request waveform transfer.

Apply the operation through the same mode owner used by local controls. An actual transition increments mode_epoch once and queues current initialized mode data/Status. Selecting the current mode succeeds without increment. SUCCESS means that action committed; a later local button/automatic transition can supersede it. Current mode is obtained from subsequent live headers/Status, not inferred indefinitely from the request. A phone display-view selection SHALL NOT itself send SetBoxMode without the explicit control action.

### 8.5 Settings

Capability bit 8 is required. GET_SETTING reads a registered setting; its successful result echoes the ID and current value. SET_SETTING validates the whole ID/value before change and returns SUCCESS only after the registered operation completes, including persistence if that setting's contract requires it. It does not imply that all settings are persistent. Unknown IDs or read/write operations not supported for a setting return UNSUPPORTED; known IDs with invalid values return INVALID_ARGUMENT.

Booleans use i32 values0/1. Enums and other signed integer settings require their registered legal values and units. Do not transmit C/C++ enum ordinal assumptions or phone-local display preferences as box setting IDs. Strings/other representations require a reviewed extension.

### 8.6 Manual calibration

Capability bit 10 is required. This is an explicit user action invoking the verified existing local manual-calibration routine and its normal preparation checks. The protocol does not invent calibration targets, required wiring, algorithm or persistence semantics. Those remain the device's documented procedure.

Once accepted, send IN_PROGRESS for that request and, after actual completion, one final SUCCESS or error for the **same opcode/ID**. Repeated IN_PROGRESS updates are allowed as activity updates; they have no percentage/detail bytes. Immediate rejection may return an error without an IN_PROGRESS. A synchronous completed calibration may return SUCCESS directly. Do not block a BLE callback/interrupt waiting for calibration to finish. Publish measurements only when they are valid; do not relabel pre-calibration samples as newly acquired data during the operation. Normal measurement publication resumes from current data afterwards.

The app keeps the request pending after IN_PROGRESS. No fixed calibration duration is claimed by this protocol. On ordinary command timeout, missing final result or disconnect, the outcome is **unknown**, not success/failure inferred from ATT. The phone SHALL NOT automatically repeat calibration or a settings write. After reconnect, read settings/observe actual mode again and use the local box display/procedure to establish calibration state. There is no calibration query/cancel command or persistent completion replay in 2.0.

## 9. Settings registry

**No production setting IDs are assigned in this revision.** The references do not define the current wire values/ranges of box display settings. Lame Display is a display setting, not an acquisition mode; Advanced Mode only changes cycle order and SHALL NOT be exposed by this registry. An app-local display preference is not automatically a box setting.

Reserve ID`0x0000`. ID`0x0102` in examples is synthetic and SHALL NOT be treated as a real setting. Until a shared registry revision assigns at least one setting, firmware keeps capability bit 8 clear and GET/SET_SETTING return UNSUPPORTED. This allows the fully specified common codec and other controls to be implemented without incompatible guesses.

To add a real setting, update this shared specification for both sides with its stable u16 ID, name, meaning, legal i32 values/units, read/write access, default, persistence and any mode/calibration interaction. Clarifying an optional registry without changing the fixed wire format increments the specification revision; enabling capabilities/settings on an existing client requires a declared compatible shared registry. Any wire layout/meaning change requires a protocol version change and updated fixtures. No side may privately assign a production ID.

Manual calibration may likewise keep bit 10 clear until its existing local routine can be bound to the defined completion/error behavior; that does not block the stream codec or SetBoxMode implementation.

## 10. Session and device behavior

1. Connect/discover service and read Public Protocol. Reject unsupported version/layout.
2. Establish authenticated encryption; negotiate ATT MTU/DLE using the platform/stack. Configure receive handlers before subscriptions.
3. Read/verify all Full Identity bytes and expected device ID. Use boot ID plus a new local connection generation to isolate state.
4. Enable Command Result indications, then Status and supported mode notifications. Check every CCCD completion. Restored bonded CCCDs may already be enabled; do not assume subscriptions always begin at zero.
5. The active mode publishes initialized data when subscribed; CCCDs are sufficient. There is no custom startup command. The receiver may discard protected notifications received before identity verification and await the immediate/current next state.
6. On disconnect, invalidate current live readings, reset reassembly and request state, and reject old callbacks. A pending command becomes outcome unknown. Same-boot reconnect preserves boot-local counters but resets per-connection IDs/sequences; fresh identity and state are required.

Switching boxes closes the previous session/reconnect work before connecting another. Do not merge devices by short name or MAC. Local testing/controls work independently of a phone.

### 10.1 Publication and freshness

Cable/Weapon/Probe use absolute30 Hz deadlines, e.g. repeating33333/33333/33334 µs intervals. A fixed33 ms timer is not exactly 30 Hz. Skip expired deadlines rather than send catch-up bursts. Keep only a newest pending periodic snapshot; complete or abandon its fragments before its replacement. No per-channel validity mask or partial Cable publication is introduced under load.

Lights uses section7.4's state-change FIFO and1 Hz refresh. Keep it responsive to irregular events; do not delay it until a resistance-publication timer. Quiet Lights is normal. Status is independently1 Hz and repeats existing diagnostics rather than driving acquisition.

The phone uses arrival monotonic time for presentation age. Periodic measurements are delayed after 100 ms and stale after 250 ms without a complete mode message. Lights becomes delayed after 3 s and stale after 5 s without a complete change/refresh state; unchanged refresh counts as current state. Status becomes delayed after 3 s and unhealthy after 5 s without a complete initialized heartbeat. Before a stream's first valid state, display waiting/unknown. Status alone cannot refresh measurements or a missed Lights state. On disconnect, mark disconnected immediately instead of waiting for these timers.

These are presentation policies, independent of the 4 s BLE supervision timeout. p95 change-to-visible latency≤100 ms is the hardware acceptance target for unsmoothed/visible or hardware-latched states. Existing resistance filtering adds its own response delay, and multiple sub-frame Lights events may not each draw on the phone.

### 10.2 Errors and overload

Drop malformed values/messages without turning them into zeros, OPEN or command success. Maintain bounded buffers and receive diagnostics. Reserved bits, unsupported type/mode, wrong lengths, invalid ADC/sentinel values, stale session identity and inconsistent fragments never refresh live state. Do not claim that sequence gaps count missing ADC acquisitions or exact scheduled-publication loss.

Reserve transmit opportunity for short Command Results. Coalesce old periodic snapshots as whole messages; apply the bounded Lights FIFO policy for state changes. No acquisition/edge interrupt SHALL wait for BLE credits. After an unknown command outcome, prefer observing current state over automatic retries.

## 11. Shared byte examples

The companion [protocol_v2_examples.json](ble/protocol_v2_examples.json) is language-neutral synthetic test data for firmware and phone codec tests. It includes complete Identity bytes, decoded expected values, field schemas extracted from these tables, logical stream bytes, fragments at small/large value limits, rejected/incomplete fragment cases and compact commands. Expected `u64` values are decimal strings so JSON consumers do not lose precision; other scalar values are JSON numbers. These are generated examples, not hardware captures. Run [check_protocol_examples.py](ble/check_protocol_examples.py) to check lengths, offsets, example bytes, inferred fragmentation and this document's tables.

Public Protocol: `54 54 42 4C 02 00 01 00`.

### 11.1 Combined Cable

Example fields: mode 0, epoch3, sequence7, uptime12500 ms, snapshot42, fault mask`0x010`; AA420/peak800, BB5600/peak6100, CC350/peak550 mΩ. ADC order is38/4095/4095/4095/502/4095/4095/4095/31. Transport ID7, logical length 60.

At V66 (e.g. ATT MTU185/247), the entire notification is:

```hex
07 00 00 01 3C 00 02 10 00 00 07 00 D4 30 00 00 2E 00 03 00 2A 00 10 00 A4 01 00 00 20 03 00 00 E0 15 00 00 D4 17 00 00 5E 01 00 00 26 02 00 00 26 00 FF 0F FF 0F FF 0F F6 01 FF 0F FF 0F FF 0F 1F 00
```

At V20 (ATT MTU23), send these five values in order:

```hex
07 00 00 05 3C 00 02 10 00 00 07 00 D4 30 00 00 2E 00 03 00
07 00 01 05 3C 00 2A 00 10 00 A4 01 00 00 20 03 00 00 E0 15
07 00 02 05 3C 00 00 00 D4 17 00 00 5E 01 00 00 26 02 00 00
07 00 03 05 3C 00 26 00 FF 0F FF 0F FF 0F F6 01 FF 0F FF 0F
07 00 04 05 3C 00 FF 0F 1F 00
```

### 11.2 Compact commands and results

These are complete characteristic values; do not add either stream header.

| Example | Bytes |
| --- | --- |
| SetBoxMode Probe, request`0x1234` | `01 34 12 04` |
| Mode action succeeded | `01 34 12 00` |
| Manual calibration, request`0x1235` | `04 35 12` |
| Calibration in progress | `04 35 12 06` |
| Calibration completed | `04 35 12 00` |
| GET_SETTING, synthetic ID`0x0102`, request`0x1236` | `02 36 12 02 01` |
| Setting read returned value3 | `02 36 12 00 02 01 03 00 00 00` |
| SET_SETTING, synthetic ID`0x0102`, value3, request`0x1237` | `03 37 12 02 01 03 00 00 00` |

The setting vectors test byte layout only; the ID is unassigned and a current production endpoint returns UNSUPPORTED. The all-options synthetic Identity fixture is also a layout fixture, not proof that optional firmware settings/calibration are implemented.

## 12. Conformance checklist

Both implementations SHALL test the same shared vectors and independently verify decoded field meanings, not only byte counts:

- Exact UUID/property/CCCD discovery; encrypted access and Public Protocol/Identity version checks; complete68-byte Identity at MTU23.
- All field offsets, signed resistance and OPEN, ADC0/4095, prohibited sentinel/reserved bits, fixed payload lengths and characteristic/type/mode combinations.
- Cable at V20/50/64/66; ATT MTU23/53/67/69/185/247; inferred first-fragment slice size; missing/duplicate/out-of-order/inconsistent fragments; maximum512-byte allocation bound and100 ms/2 s deadlines.
- Sequence, snapshot ID, mode epoch and uptime wrap; same-mode return after an intervening mode; reconnect at a nonzero epoch/counter; callbacks from a previous box/session.
- Lights initial state, handler changes, both weapons/latches, activation/release before a display frame, unchanged refresh with the same snapshot ID, quiet periods, bounded FIFO overload and refresh recovery.
- All short command/result lengths and statuses; unsupported capability/setting; strictly increasing request IDs; duplicate ID without re-execution; local mode precedence; calibration progress/final result; disconnect/timeout with no automatic repeat.
- Real-device30 Hz complete Cable/Weapon/Probe delivery and event-driven Lights latency, with MTU and DLE varied separately. Codec vectors alone are not radio/timing/security evidence.

Unimplemented optional settings/calibration keep their capability bits clear. Capture/history reserved bits and message types remain disabled until a separately versioned common contract exists. Update this document and shared vectors together for any protocol change.

## 13. Reference material

The [implementation plan](BLE_IMPLEMENTATION_PLAN.md) contains resource planning, firmware dependencies and the detailed standards review. Bluetooth behavior follows [ATT](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/host/attribute-protocol--att-.html), [GATT](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/host/generic-attribute-profile--gatt-.html) and [Assigned Numbers](https://www.bluetooth.com/specifications/assigned-numbers/). Platform MTU handling follows [Android BluetoothGatt](https://developer.android.com/reference/android/bluetooth/BluetoothGatt) and [Apple Core Bluetooth guidance](https://devstreaming-cdn.apple.com/videos/wwdc/2017/712jqzhsxoww3zn/712/712_whats_new_in_core_bluetooth.pdf). The 247/251 starting configuration is documented in the plan; larger negotiated capacity does not change the 66-byte Cable value.
