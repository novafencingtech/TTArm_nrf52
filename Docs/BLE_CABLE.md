# Cable messaging

Implements only the combined Cable stream from the revision 2 message
specification. Weapon, Probe, Lights and commands remain deferred.

Connect/encrypt, select Cable mode (Cord or Lame presentation), and enable
notifications on `4a1e0010-3bb5-44b4-b566-8d22e72da6f5`. Status subscriptions
remain independent. The Full Identity capability mask is now `0x00000201`.

After a completed cable update, the firmware caches the fault mask, filtered
AA/BB/CC resistances, existing decaying peaks, and nine matrix ADC values.
There is no additional acquisition or reset. The six-line main sketch hook
copies the completed foreground state; ADC interrupt work is unchanged.
Resistances use signed milliohms, with the existing open-circuit threshold
encoded as `0x7FFFFFFF`. Invalid numeric/ADC values withhold publication.

Cable publishes on absolute 30 Hz deadlines (33333/33333/33334 microseconds),
skipping missed slots. Snapshot IDs survive reconnects and mode transitions;
sequence/transport IDs reset per connection. Subscription copies reuse the
current snapshot ID and set the immediate flag. Mode changes discard pending
fragments and require a new completed cable update before publication.

The logical frame is 60 bytes: a 14-byte header and 46-byte payload. Every
notification has the existing six-byte transport wrapper. At MTU 247 (still
requested), each complete Cable message is one 66-byte notification. At MTU 23,
five notifications have lengths 20/20/20/20/10. Retries preserve bytes and slice
boundaries; pending frames expire after 100 ms. Queue backpressure never waits.
Only one pending frame and the latest cache are kept, without a backlog.

The basic nRF52832 compilation check passed. On November 1, 2026, the user
confirmed that Cable messaging is verified on hardware.
The implementation uses the same stock APIs for nRF52832 and nRF52840.
Further hardware qualification can check decoded values against the local
display/serial output, complete delivery rate, MTU 23 fragmentation, mode
exit/reentry, disconnect/reconnect and normal acquisition under radio load.
