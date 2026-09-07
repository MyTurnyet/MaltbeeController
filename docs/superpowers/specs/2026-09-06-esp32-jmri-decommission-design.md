# ESP32 JMRI Bridge Decommissioning (Sub-project #9c) — Design

This is sub-project **#9c**, the last piece of the Loco2MQTT pivot (see
`docs/superpowers/specs/2026-09-06-esp32-loco2mqtt-turnout-bridge-design.md`
for #9a and `docs/superpowers/specs/2026-09-06-esp32-presence-heartbeat-design.md`
for #9b, both already merged). It removes the now-orphaned JMRI-side bridge
script and reconciles the two documentation files that still describe the
retired JMRI-over-MQTT integration as current.

Unlike #9a/#9b, this sub-project touches **no `lib/`/`src/`/`test/` C++
code** — every native/`esp32dev` build gate is unaffected by construction.
The work is one file deletion plus three documentation rewrites.

## Scope decisions (from user Q&A during brainstorming)

- **Delete `jmri/panel_mqtt_turnout_bridge.py` outright** (not archived or
  renamed-obsolete). It's fully orphaned — nothing in `lib/`/`src/`/`test/`
  has referenced it since #9a — and git history preserves it if anyone
  ever needs to see it again.
- **Rewrite `docs/ESP32_Turnout_Panel_Implementation.md`'s JMRI-specific
  content in place**, not just banner it as superseded. Its hardware
  wiring/GPIO tables (still accurate) stay untouched; its JMRI Communication
  section, comparison table, decision log, and per-turnout config example
  get rewritten to describe Loco2MQTT.
- **Rewrite `docs/HARDWARE_BRINGUP_CHECKLIST.md`'s remaining JMRI mentions**
  (§2.1 Prerequisites, one phrase in §2.3 — §2.2 and the rest of §2.5 were
  already fixed by #9a/#9b's own whole-branch-review fixes). The
  "Status as of 2026-09-02" note recording that two real boards were
  verified stays, with an added caveat that the verification was against
  the now-replaced JMRI setup and hasn't been re-run against Loco2MQTT.

## File 1: Delete `jmri/panel_mqtt_turnout_bridge.py`

Delete the file and the now-empty `jmri/` directory.

## File 2: `CLAUDE.md`

**A.** Find the `jmri/panel_mqtt_turnout_bridge.py` bullet (currently a long
paragraph describing the script's internals, prefixed "Orphaned as of
sub-project #9a... Left in place, undeleted, pending its own removal in a
future sub-project (#9c)"). Replace the entire bullet with:

```
- `jmri/panel_mqtt_turnout_bridge.py` — **removed in sub-project #9c.** This
  was the JMRI-side Jython startup script that bridged MQTT to JMRI
  `Turnout` objects for the ESP32 panel's original JMRI-over-MQTT
  integration (`track/turnout/<jmriSystemName>` topics, no shadow "MT"
  turnouts or Logix). Orphaned by sub-project #9a's migration to Loco2MQTT
  (nothing in `lib/`/`src/`/`test/` referenced it any more) and deleted
  here. See `docs/ESP32_Turnout_Panel_Implementation.md`'s git history if
  you need to see what it did.
```

**B.** In the "Loco2MQTT turnout bridge (sub-project #9a)" section, find:
```
`jmri/panel_mqtt_turnout_bridge.py` is now orphaned pending removal in a
future sub-project (#9c) — nothing in `lib/`/`src/`/`test/` references it
any more.
```
Replace with:
```
`jmri/panel_mqtt_turnout_bridge.py` was orphaned by this migration and
removed in sub-project #9c.
```

## File 3: `docs/ESP32_Turnout_Panel_Implementation.md`

**A. "## Status" section.** Replace the whole section (from `## Status`
through the end of the "Note on class names below" paragraph, i.e.
everything up to but not including `## Relationship to the Mega/LocoNet
system`) with:

```
## Status

**Implemented and merged to `main`.** This document originally captured the
hardware design and software plan worked out in a ChatGPT conversation
(2026-07-21); since then all 6 "Suggested milestones" below have shipped —
matrix scanning, LED-pair driving, ESP32 hardware adapters, MQTT transport,
the `src/esp32/main.cpp` composition root, wireless commissioning,
multi-panel presence/collision detection, and identify-blink are all
programming-complete (see `CLAUDE.md` for the authoritative, currently-
maintained architecture description — this doc is kept for the hardware
design rationale and wiring tables, which haven't changed; its MQTT/topic
content was rewritten in sub-project #9c to match the current Loco2MQTT
design after sub-project #9a replaced JMRI). Only the physical hardware
bring-up pass remains — see `docs/HARDWARE_BRINGUP_CHECKLIST.md`.

The panel talks directly to Loco2MQTT, a companion device that bridges
LocoNet to MQTT via its own on-device broker — see "Loco2MQTT Communication
(MQTT)" below. The original design (JMRI over MQTT via an external broker
and a JMRI-side bridge script, `jmri/panel_mqtt_turnout_bridge.py`) was
replaced in sub-project #9a and that script was removed in sub-project #9c.

**Note on class names below:** the "Suggested per-turnout config" and
`MqttJmriTurnoutCommandAdapter`/`MqttJmriFeedbackSource`/`JmriCommandEncoder`/
`JmriFeedbackDecoder` names in this doc were the pre-implementation proposal.
The classes that actually shipped were named `JmriTurnoutCommandAdapter` and
`JmriFeedbackSource` (renamed to `Loco2MqttTurnoutCommandAdapter`/
`Loco2MqttFeedbackSource` in sub-project #9a), with the topic/payload
construction factored into standalone `TopicScheme` and `PayloadCodec` classes
(`lib/McsEsp32/src/domain/`) rather than separate encoder/decoder classes —
same native-testable-core-plus-thin-adapter shape as proposed, different
names/decomposition.
```

**B. Comparison table.** In "## Relationship to the Mega/LocoNet system",
find the table row/cells:
```
| Turnout command transport | LocoNet (direct to DR5000) | Wi-Fi (MQTT) to JMRI |
```
and
```
| Feedback | LocoNet feedback (Milestone 10) | JMRI-confirmed state via MQTT (this doc) — LEDs reflect JMRI's reported state, not just what was commanded |
```
Replace with:
```
| Turnout command transport | LocoNet (direct to DR5000) | Wi-Fi (MQTT) to Loco2MQTT |
```
and
```
| Feedback | LocoNet feedback (Milestone 10) | Loco2MQTT-confirmed state via MQTT (this doc) — LEDs reflect Loco2MQTT's reported state, not just what was commanded |
```

**C. "## Project Goal" section.** Find:
```
- An MQTT command sent to JMRI when the button is pressed
- LEDs that show JMRI's **confirmed** state for that turnout — updated only once
  JMRI publishes the resulting state back over MQTT (not optimistically on
  button press), and updated the same way when the turnout changes for any
  other reason (another panel, PanelPro, a dispatcher) — see "JMRI
  Communication (MQTT)" below
```
Replace with:
```
- An MQTT command sent to Loco2MQTT when the button is pressed
- LEDs that show Loco2MQTT's **confirmed** state for that turnout — updated
  only once Loco2MQTT publishes the resulting state back over MQTT (not
  optimistically on button press), and updated the same way when the
  turnout changes for any other reason (another panel, another controller,
  physical feedback) — see "Loco2MQTT Communication (MQTT)" below
```

**D. Controller hardware bullet.** Find:
```
- Wi-Fi used continuously to talk to JMRI
```
Replace with:
```
- Wi-Fi used continuously to talk to Loco2MQTT
```

**E. Power section.** Find:
```
- This ESP32 does **not** power turnout motors/decoders — it only handles
  button input, LED indication, and Wi-Fi/JMRI communication.
```
Replace with:
```
- This ESP32 does **not** power turnout motors/decoders — it only handles
  button input, LED indication, and Wi-Fi/Loco2MQTT communication.
```

**F. Wireless Setup Access Point section.** Find:
```
open the AP on its own; without the BOOT-button hold it boots waiting for
bench-serial commissioning instead (see "JMRI Communication (MQTT)" and
the bench-serial commissioning design for that path).
```
Replace with:
```
open the AP on its own; without the BOOT-button hold it boots waiting for
bench-serial commissioning instead (see "Loco2MQTT Communication (MQTT)"
and the bench-serial commissioning design for that path).
```
Also find:
```
The panel's own current WiFi password is never displayed by the setup
form — a blank password field on submission keeps the existing password
unchanged. Turnout JMRI name fields work the opposite way: a blank field on
submission clears that turnout's assigned name.
```
Replace with:
```
The panel's own current WiFi password is never displayed by the setup
form — a blank password field on submission keeps the existing password
unchanged. Turnout address fields work the opposite way: a blank field on
submission clears that turnout's assigned address.
```

**G. Identifying a Physical Panel section.** Find:
```
to make that one physical panel's LEDs flash so you can find it among
several deployed panels — useful when verifying commissioning worked,
debugging via JMRI/MQTT tooling, or telling apart two panels mid-collision
```
Replace with:
```
to make that one physical panel's LEDs flash so you can find it among
several deployed panels — useful when verifying commissioning worked,
debugging via MQTT tooling, or telling apart two panels mid-collision
```

**H. Replace the entire "## JMRI Communication (MQTT)" section** (from its
heading through the end of the "JMRI-side bridge script" subsection, i.e.
everything up to but not including `## Software responsibilities`) with:

```
## Loco2MQTT Communication (MQTT)

**Decision (2026-07-21, migrated to Loco2MQTT 2026-09-06 — sub-project
#9a):** the ESP32 talks directly to Loco2MQTT — a companion ESP32 device
that bridges the layout's LocoNet bus to MQTT via its own on-device broker
— both to send turnout commands and to receive turnout state-change
notifications. This replaced an earlier design where the panel talked to
JMRI over MQTT via an external broker and a JMRI-side bridge script; that
path is fully retired (`jmri/panel_mqtt_turnout_bridge.py` was removed in
sub-project #9c). MQTT itself was chosen over JMRI's WebSocket JSON server,
the WiThrottle/simple TCP protocol, and HTTP polling because it gives a
persistent, push-based connection for feedback (no polling) with a
comparatively simple client library on the ESP32 side — that reasoning
carried over unchanged to the Loco2MQTT design.

This reuses the existing Mega/LocoNet application layer almost unchanged —
`TurnoutCommandPort`, `TurnoutControl`, and `TurnoutFeedback`
(`lib/McsCore/src/ports/TurnoutCommandPort.h`,
`lib/McsCore/src/application/TurnoutControl.h`) are already transport-agnostic.
The ESP32-specific work is adapters that plug into those same interfaces,
mirroring the pattern already used for LocoNet on the Mega side
(`MrrwaLocoNetTurnoutAdapter` / `LocoNetFeedbackDecoder`):

- **`Loco2MqttTurnoutCommandAdapter`** (`lib/McsEsp32/src/adapters/`)
  implements `TurnoutCommandPort::send()`. Looks up the turnout channel's
  configured LocoNet address from `NodeConfig::channelTurnoutAddresses` and
  publishes the command over MQTT. Topic construction is factored into a
  pure, native-testable `TopicScheme` class (`lib/McsEsp32/src/domain/`) so
  only the actual MQTT `publish()` call is untestable off-hardware.
- **`Loco2MqttFeedbackSource`** — a poll-shaped port matching
  `LocoNetFeedbackSource::poll()`. The MQTT client library's subscribe
  callback pushes incoming `{topic, payload}` messages into a small queue;
  `poll()` drains one per call, non-blocking.
- **`PayloadCodec`** (`lib/McsEsp32/src/domain/`) — native-testable,
  encodes/decodes the `CLOSED`/`THROWN` payload words.
- **12× `TurnoutControl`**, unmodified, one per turnout, each wired to its
  button pair, LED-pair indicator, and the shared
  `Loco2MqttTurnoutCommandAdapter`.

### Data flow

**Command (panel → Loco2MQTT):** button press → `TurnoutControl::update()`
(unmodified) → `turnoutCommandPort_.send(address, position)` →
`Loco2MqttTurnoutCommandAdapter` publishes to `loconet/turnout/<address>/set`.
`update()` never touches the indicator, so the LED does not change at this
point.

**Feedback (Loco2MQTT → panel):** Loco2MQTT publishes a turnout's state on
`loconet/turnout/<address>/state` — whether that's because of this panel's
command or because it changed for any other reason on the layout (another
panel, a physical throw with feedback) — and re-publishes every known
turnout's state unconditionally every 30 seconds as its own workaround for
not honoring the MQTT retained flag. The ESP32 (subscribed to each
configured turnout's own state topic) queues the message via
`Loco2MqttFeedbackSource`; each `loop()` the composition root drains the
queue and calls the matching turnout's `TurnoutControl::applyFeedback(feedback)`
— which is the only place `indicator_.display()` gets called. A repeated
identical feedback message (from the 30-second re-publish) is a harmless
no-op here.

### Connection loss and reconnection

Unchanged from the original design: `TurnoutIndicator::clear()` is the hook
for "no confirmed state," and the ESP32 LED-pair driver treats "both off"
as blink mode. Startup, disconnect, and reconnect all behave exactly as
described below in "Startup sequence" — none of that logic depended on
which broker/backend was on the other end.

**State publishes are not retained** — this is now inherent to Loco2MQTT's
broker, which ignores the retained flag entirely (rather than a deliberate
choice on the JMRI-bridge side, as it was originally). A panel that
reconnects mid-session stays in blink/unconfirmed state for a given turnout
until that turnout's state next actually changes, or until the next
30-second unconditional re-publish arrives (see "Data flow" above) —
whichever comes first.

### Broker discovery

Loco2MQTT advertises itself via mDNS as `loco2mqtt.local`. The panel
resolves this once at boot (`EspMdnsResolver`/`BrokerAddressResolver`,
`lib/McsEsp32/src/`), falling back to a manually-configured broker
host/port if mDNS resolution fails. See `CLAUDE.md`'s "Loco2MQTT turnout
bridge (sub-project #9a)" section for the full contract and known
limitations (no periodic re-resolution after boot, etc.).
```

**I. "## Software responsibilities" numbered list.** Find:
```
2. Connect to / communicate with the JMRI server.
```
Replace with:
```
2. Connect to / communicate with Loco2MQTT.
```
Find:
```
8. Send the turnout command to JMRI over MQTT.
9. Subscribe to JMRI's turnout state topic(s) and decode incoming feedback.
10. Store/retrieve the last JMRI-confirmed turnout state (not the commanded
    state — see "JMRI Communication (MQTT)" above).
11. Drive the LED GPIO: HIGH for green, LOW for red, only once JMRI confirms
    the state — never optimistically on button press.
```
Replace with:
```
8. Send the turnout command to Loco2MQTT over MQTT.
9. Subscribe to Loco2MQTT's turnout state topic(s) and decode incoming feedback.
10. Store/retrieve the last Loco2MQTT-confirmed turnout state (not the commanded
    state — see "Loco2MQTT Communication (MQTT)" above).
11. Drive the LED GPIO: HIGH for green, LOW for red, only once Loco2MQTT confirms
    the state — never optimistically on button press.
```

**J. "### Suggested per-turnout config" struct.** Replace the whole
subsection (code block plus its explanatory paragraph) with:
```cpp
struct TurnoutConfig {
    int turnoutNumber;
    int matrixRow;
    int matrixColumn;
    int ledGpio;
    int locoNetAddress;
};

TurnoutConfig turnouts[] = {
    {1, 0, 0, 4,  5},
    {2, 0, 1, 13, 6},
    {3, 0, 2, 14, 7},
    {4, 0, 3, 16, 8},
    {5, 1, 0, 17, 9},
    {6, 1, 1, 22, 10},
    {7, 1, 2, 23, 11},
    {8, 1, 3, 25, 12},
    {9, 2, 0, 26, 13},
    {10, 2, 1, 27, 14},
    {11, 2, 2, 32, 15},
    {12, 2, 3, 33, 16}
};
```

`locoNetAddress` values above are placeholders — must be set to match the
real layout's LocoNet turnout addresses before deployment. `locoNetAddress`
is also the value substituted into the MQTT command topic
(`loconet/turnout/<address>/set`) and state topic
(`loconet/turnout/<address>/state`) for that turnout — no separate topic
fields are needed in this struct.

**K. "### Startup sequence".** Find:
```
5. Connect to JMRI over MQTT; subscribe to turnout state topic(s).
6. Wait for JMRI to publish confirmed state for each turnout (each arrival
   calls that turnout's `TurnoutControl::applyFeedback()`, which stops that
   LED's blinking and shows the confirmed color) — turnouts JMRI hasn't
   reported yet keep blinking.
```
Replace with:
```
5. Connect to Loco2MQTT over MQTT (resolved via mDNS, falling back to a
   manually-configured broker host); subscribe to turnout state topic(s).
6. Wait for Loco2MQTT to publish confirmed state for each turnout (each
   arrival calls that turnout's `TurnoutControl::applyFeedback()`, which
   stops that LED's blinking and shows the confirmed color) — turnouts not
   yet reported keep blinking.
```

**L. "## Open questions / follow-ups" checklist.** Replace the whole
checklist (all `- [x]` items) with:
```
- [x] Turnout MQTT communication protocol/transport — decided 2026-07-21:
      MQTT; migrated 2026-09-06 (sub-project #9a) from an external broker +
      JMRI-side bridge script to Loco2MQTT's on-device broker (see
      "Loco2MQTT Communication (MQTT)" above).
- [x] `UNKNOWN`-state handling — decided 2026-07-21: blink last-known/default
      color (see State Model above). Unaffected by the #9a migration.
- [x] Exact MQTT topic structure and payload format — implemented as
      `loconet/turnout/<address>/set` (command) and
      `loconet/turnout/<address>/state` (state), payload `"THROWN"` or
      `"CLOSED"` in both directions (unchanged from the original design's
      payload words — only the topic prefix and the value keying the
      address changed). See `TopicScheme`/`PayloadCodec`
      (`lib/McsEsp32/src/domain/`).
- [x] Whether turnout state is published as retained — resolved above under
      "Connection loss and reconnection": on-change plus an unconditional
      30-second re-publish, never retained (Loco2MQTT's broker ignores the
      retained flag entirely).
- [x] MQTT broker — as of sub-project #9a, the broker is Loco2MQTT itself
      (a companion ESP32 device with its own on-device broker), discovered
      via mDNS (`loco2mqtt.local`) with a manual-host fallback. No longer a
      separate Mosquitto-style broker + JMRI.
- [x] Real LocoNet turnout addresses for each channel — assigned during
      commissioning (`turnout N address <address>`), not hardcoded in
      `main.cpp` — same pattern the original JMRI-name assignment used,
      just an integer instead of a system-name string.
- [x] Verify GPIO 4 boot behavior on the actual ELEGOO board — confirmed
      2026-09-02: turnout 12 wired and working (throws/closes correctly,
      LEDs correct), and the board has been power-cycled multiple times
      with no stray flash/glitch on turnout 12's LED pair during boot.
      (This verification predates the #9a migration and was against the
      original JMRI setup — see `docs/HARDWARE_BRINGUP_CHECKLIST.md` for
      the current bring-up status.)
- [x] Confirm ESP32 board power behavior before ruling out external 5V/VIN —
      confirmed 2026-09-02: this board runs exclusively off 5V/VIN with no
      USB connected (and none intended for permanent deployment), working
      correctly.
```

**Not touched:** wiring tables, GPIO assignment tables, button matrix,
LED wiring, power section's non-JMRI content, physical assumptions,
"Suggested milestones" (explicitly headed "all shipped — kept for
history," describing the original pre-implementation plan as a historical
record, not current-state documentation).

## File 4: `docs/HARDWARE_BRINGUP_CHECKLIST.md`

**A. §2.1 Prerequisites.** Replace the whole subsection with:
```
### 2.1 Prerequisites

- A Loco2MQTT device (see its own project documentation) running and
  reachable from the ESP32 panel's WiFi network — either discoverable via
  mDNS as `loco2mqtt.local`, or its IP/port known for manual configuration
  as a fallback.
- LocoNet turnout addresses decided for at least a few channels (the panel
  doesn't require all 12 to be configured to boot — partial commissioning
  is explicitly supported).
- The ELEGOO ESP32 board wired per `docs/ESP32_Turnout_Panel_Implementation.md`'s
  GPIO Assignment section (3×4 button matrix, 12 LED pairs) — wire
  however many turnouts you're bringing up. The wireless-setup gesture
  (2.4) uses the board's own onboard BOOT button, not any turnout wiring,
  so no turnout needs to be wired first just to exercise it.
```

**B. §2.3 Matrix buttons, LEDs, and turnout control.** Find:
```
For each turnout channel you've wired and named in commissioning:

- Press its button. LED should update once JMRI confirms the state (not
  optimistically on press) — expect a brief delay, not instant.
- Confirm the LED blinks (unconfirmed/disconnected state) if you
  temporarily stop the MQTT broker, and resumes normal display once it's
  back.
```
Replace with:
```
For each turnout channel you've wired and addressed in commissioning:

- Press its button. LED should update once Loco2MQTT confirms the state
  (not optimistically on press) — expect a brief delay, not instant.
- Confirm the LED blinks (unconfirmed/disconnected state) if you
  temporarily stop Loco2MQTT, and resumes normal display once it's back.
```

**C. Part 2's "Status as of 2026-09-02" note.** Find:
```
**Status as of 2026-09-02:** two real ESP32 boards are built and
commissioned against a live MQTT broker/JMRI. Confirmed working: 2.1
(broker running, real JMRI names assigned), 2.2 (flash/boot/commissioning),
2.3 (button → LED → JMRI turnout control, including turnout 12/GPIO4 —
wired, working, and power-cycled multiple times with no boot-time LED
glitch), and 2.4 (the BOOT-button wireless setup gesture, including
joining the now-open setup AP and submitting the form). One board also
runs entirely off 5V/VIN with no USB connected, confirming the
external-power open question. **Still outstanding:** 2.5
(presence/collision — feasible now with two boards, just not yet
exercised) and 2.6 (identify-blink).
```
Replace with:
```
**Status as of 2026-09-02** (predates sub-project #9a's JMRI→Loco2MQTT
migration — this verification was against the original JMRI setup and has
not been re-run against Loco2MQTT; §2.1/§2.2/§2.3 below now describe the
current Loco2MQTT-based procedure, not what was actually exercised on
this date): two real ESP32 boards were built and commissioned against a
live MQTT broker/JMRI. Confirmed working: 2.1 (broker running, real JMRI
names assigned), 2.2 (flash/boot/commissioning), 2.3 (button → LED → JMRI
turnout control, including turnout 12/GPIO4 — wired, working, and
power-cycled multiple times with no boot-time LED glitch), and 2.4 (the
BOOT-button wireless setup gesture, including joining the now-open setup
AP and submitting the form). One board also ran entirely off 5V/VIN with
no USB connected, confirming the external-power open question. **Still
outstanding:** 2.5 (presence/collision — feasible now with two boards,
just not yet exercised) and 2.6 (identify-blink), plus **re-verifying
2.1-2.3 against the current Loco2MQTT procedure**, which has not been
tested on real hardware yet.
```

**Not touched:** §2.2 (already correct — fixed by #9a/#9b), §2.4, the rest
of §2.5, §2.6, and Part 1 (Mega/LocoNet — unrelated to this migration).

## Testing

None — this sub-project changes no `lib/`/`src/`/`test/` code. Verification
is: `pio test -e native` and `pio run -e esp32dev` both still succeed
(trivially true, since nothing they compile changed) — run once at the end
as a sanity check that the deletion didn't somehow break a build reference,
plus a final grep sweep confirming no remaining stale JMRI-as-current-design
claims in the four files this sub-project touches.

## Out of scope for this slice

- Any change to `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`
  or the Mega/LocoNet system — unrelated, not JMRI/Loco2MQTT.
- `docs/superpowers/specs/` and `docs/superpowers/plans/` — historical
  design/plan records correctly describing what was true at the time they
  were written; not touched, matching the convention already established
  by #9a/#9b's own whole-branch reviews (which explicitly confirmed grep
  hits in these directories are accurate history, not staleness).
- `docs/Refactoring_Recommendations_Multi_Hardware.md` — a pre-ESP32-work
  historical planning document with two passing JMRI mentions in an
  unrelated context (Wi-Fi/JMRI client library scoping under
  `platformio.ini`); not current-state documentation, not touched.
- Re-running physical hardware verification against the current Loco2MQTT
  procedure — noted as still-outstanding in the updated bringup-checklist
  status note, not performed by this sub-project.
