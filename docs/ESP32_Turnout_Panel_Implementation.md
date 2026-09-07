# ESP32 Turnout Panel — Implementation Tracking

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

## Relationship to the Mega/LocoNet system

This is a **separate hardware platform** from the Mega 2560 / LocoNet system
described in `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`:

| | Mega 2560 panel (existing) | ESP32 panel (this doc) |
|---|---|---|
| MCU | Arduino Mega 2560 | ESP32-WROOM-32 (ELEGOO dev board) |
| Turnout command transport | LocoNet (direct to DR5000) | Wi-Fi (MQTT) to Loco2MQTT |
| Input wiring | Discrete pins per button | 3×4 matrix (7 pins for 12 buttons) |
| LED wiring | Discrete pins per indicator LED | 1 GPIO per red/green pair (12 pins for 24 LEDs) |
| Feedback | LocoNet feedback (Milestone 10) | Loco2MQTT-confirmed state via MQTT (this doc) — LEDs reflect Loco2MQTT's reported state, not just what was commanded |

**Decision (2026-07-21):** this stays in-repo as a second PlatformIO
environment, reusing the Arduino-independent domain layer (`Button`,
`Indicator`, `Turnout`, `TurnoutService`, ...) from `lib/McsCore` behind new
ESP32-specific adapters. The button-matrix scanning and paired-LED-on-one-GPIO
wiring need new port implementations rather than reusing
`ArduinoDigitalInput`/`ArduinoDigitalOutput` 1:1 — see "PlatformIO environment
setup" under Suggested milestones below.

---

## Project Goal

Build model railroad turnout control panels using ELEGOO ESP32 development
boards. Each turnout on a panel gets:

- One momentary pushbutton
- One red LED + one green LED
- An MQTT command sent to Loco2MQTT when the button is pressed
- LEDs that show Loco2MQTT's **confirmed** state for that turnout — updated
  only once Loco2MQTT publishes the resulting state back over MQTT (not
  optimistically on button press), and updated the same way when the
  turnout changes for any other reason (another panel, another controller,
  physical feedback) — see "Loco2MQTT Communication (MQTT)" below

Design goal: maximize turnout controls per ESP32 while keeping boot reliable and
USB available for programming/debugging.

---

## Hardware

### Controller

ELEGOO ESP32 Development Board:
- ESP32-WROOM-32, USB-C, CP2102 USB-to-serial
- Wi-Fi used continuously to talk to Loco2MQTT
- USB kept connected/available for programming and serial debugging

### Per-turnout panel components

- 1 normally-open momentary pushbutton
- 1 red LED + 1 green LED (separate LEDs, not a shared bi-color lead — lets one
  GPIO drive the pair)
- 2 current-limiting resistors (one per LED — never shared)

### Capacity: 12 turnouts per ESP32

- 12 pushbuttons via a 3×4 matrix → 7 GPIOs (3 rows + 4 columns)
- 12 LED pairs → 12 GPIOs (1 per pair)
- **19 GPIOs total**

---

## GPIO Assignment

### Button matrix — rows (outputs)

| Pin | Role |
|---|---|
| GPIO 18 | Row 1 |
| GPIO 19 | Row 2 |
| GPIO 21 | Row 3 |

### Button matrix — columns (inputs)

GPIO 34, 35, 36, 39 are input-only on the ESP32 and have **no usable internal
pull-up**, so each needs an external 10 kΩ pull-up to 3.3 V.

| Pin | Role | External pull-up |
|---|---|---|
| GPIO 34 | Column 1 | 10 kΩ → 3.3 V |
| GPIO 35 | Column 2 | 10 kΩ → 3.3 V |
| GPIO 36 | Column 3 | 10 kΩ → 3.3 V |
| GPIO 39 | Column 4 | 10 kΩ → 3.3 V |

### LED pair outputs (1 GPIO per turnout)

| Turnout | GPIO |
|---|---|
| 1 | 4 |
| 2 | 13 |
| 3 | 14 |
| 4 | 16 |
| 5 | 17 |
| 6 | 22 |
| 7 | 23 |
| 8 | 25 |
| 9 | 26 |
| 10 | 27 |
| 11 | 32 |
| 12 | 33 |

### Onboard BOOT button

| Pin | Role | Wiring |
|---|---|---|
| GPIO 0 | Wireless-setup trigger (hold 3 s, then release) | None — the ESP32 dev board's own BOOT button |

GPIO 0 is still a boot-strapping pin and is read live only during `loop()`,
never at startup. **Do not hold BOOT while pressing EN or applying
power** — that puts the ROM into UART download mode instead of running
the firmware (recover by releasing BOOT and pressing EN again).

### Pins intentionally avoided

- GPIO 1 / 3 — USB serial TX/RX
- GPIO 6–11 — connected to flash memory
- GPIO 2, 5, 12, 15 — boot-strapping pins

**GPIO 4** is used as turnout 12's LED output. It isn't one of the primary
boot-strapping pins, but startup behavior should still be verified on the
actual ELEGOO board before relying on it.

---

## Button matrix

Layout:

| | Col 1 | Col 2 | Col 3 | Col 4 |
|---|---|---|---|---|
| Row 1 | T1 | T2 | T3 | T4 |
| Row 2 | T5 | T6 | T7 | T8 |
| Row 3 | T9 | T10 | T11 | T12 |

Buttons are normally open; a press connects its row to its column. Only one
button is expected to be pressed at a time, so **no diodes are required** on
individual buttons (ghosting from simultaneous presses is not a concern here).

### Scanning algorithm

Columns idle HIGH via their external pull-ups. Repeatedly, for each row:

1. Drive the active row LOW; keep other rows inactive (input/high-impedance,
   or otherwise unable to interfere).
2. Read all 4 column pins.
3. A column reading LOW means the button at (active row, that column) is
   pressed.
4. Advance to the next row and repeat continuously.

Example: Row 2 driven LOW, Column 3 reads LOW → Turnout 7 pressed.

Debouncing and edge detection (new-press only, not repeat-while-held) are
required in software — same responsibility `Button` already has on the Mega
side, just against a matrix-scanned input instead of a single `digitalRead`.

---

## LED pair wiring (1 GPIO, 2 LEDs, opposite polarity)

The two LEDs are wired in opposite directions so one GPIO state lights green
and the other lights red.

```text
GPIO ── resistor ── green LED anode → cathode ── GND

3.3V ── resistor ── red LED anode → cathode ── GPIO
```

The red LED is **not** wired directly between 3.3 V and ground — its cathode
returns through the GPIO. Each LED gets its own resistor (680 Ω–1 kΩ; 1 kΩ
recommended starting point).

| GPIO output | Green | Red |
|---|---|---|
| HIGH (~3.3V) | On | Off |
| LOW (~0V) | Off | On |

- **HIGH**: current flows GPIO → green LED → GND (green lights); both ends of
  the red LED sit at ~3.3V, no forward voltage, red stays off.
- **LOW**: green has no forward voltage, stays off; current flows 3.3V → red
  LED → GPIO (sinking), red lights.

Because the pair always shows one color or the other, there's no way to show
"both off" for an `UNKNOWN` state without a hardware or software workaround
(see State Model below).

---

## Power

- ESP32 powered via USB-C.
- ESP32 3.3V rail supplies: matrix pull-ups, and the 3.3V side of each red LED
  circuit.
- All grounds common: ESP32 GND, green LED returns, any other panel circuit
  ground.
- Do not feed 5V/VIN externally while also powered via USB unless the board's
  power-input behavior has been verified.
- This ESP32 does **not** power turnout motors/decoders — it only handles
  button input, LED indication, and Wi-Fi/Loco2MQTT communication.

---

## Wireless Setup Access Point

**Decision (2026-08-30):** commissioning over Wi-Fi (sub-project #2c) opens a
temporary access point named `MaltBee-Setup-XXXX` (last 4 hex digits of the
ESP32's MAC address) whenever the panel boots into wireless setup mode.
Wireless setup mode is entered by holding the ESP32 board's own BOOT
button (GPIO0) for 3 seconds, then releasing it — this works the same way
whether the panel is a factory-fresh board that has never been
configured, or an already-commissioned panel a technician wants to
reconfigure. No extra wiring is needed; the BOOT button is already
present on the board. A factory-fresh panel does **not** automatically
open the AP on its own; without the BOOT-button hold it boots waiting for
bench-serial commissioning instead (see "Loco2MQTT Communication (MQTT)"
and the bench-serial commissioning design for that path).

While the setup AP is open, all 12 LED pairs flash green/red together at
the same fast rate used for MQTT identify-blink (`LedPairDriver::setIdentifying()`,
sub-project #2d-b) — the two states can never overlap, since wireless
setup mode never starts MQTT.

The AP is **open** (no password required to join) — physical access to
the panel (the BOOT-button hold that opens it) is the only gate.
Wireless setup mode has no timeout, so an abandoned mid-commissioning
panel stays open to anyone in range until someone completes the form or
power-cycles the board; don't leave a panel in setup mode unattended any
longer than necessary. Because the AP itself has no encryption, avoid
changing a panel's WiFi password over the setup form while in an
untrusted RF environment — the new password is sent in cleartext over
the open AP during that submission (stored credentials are never read
back out, only ever written).

The WiFi SSID field also offers a dropdown of nearby networks (scanned
once when the AP opens, refreshable via a "Rescan" link on the page) —
selecting one fills the text field, which can still be typed into
directly for a network that isn't listed.

The panel's own current WiFi password is never displayed by the setup
form — a blank password field on submission keeps the existing password
unchanged. Turnout address fields work the opposite way: a blank field on
submission clears that turnout's assigned address.

---

## Multi-Panel Presence and Node ID Collisions

**Decision (2026-08-30, heartbeat redesign 2026-09-06 — sub-project
#9b):** every panel publishes two MQTT topics — `panel/<nodeId>/status`
(`"online"`/`"offline"`) and `panel/<nodeId>/mac` (the panel's own MAC,
last 4 hex digits) — immediately on connect **and again every 30 seconds
while connected**, so a technician with any MQTT client can see which
panels are up and which physical panel currently claims a given node ID.
Neither topic is retained (see "Why not retained" below).

**If two panels are ever accidentally commissioned with the same node
ID**, each one detects the other by watching its own `mac` topic: seeing
a MAC that isn't its own means a second panel claims this ID. When that
happens, **the panel goes visibly unresponsive** — all 12 turnout buttons
stop working and every LED falls back to the same blinking
"unconfirmed/disconnected" state already used when MQTT is down. This
looks identical to a lost network connection at a glance; check the
serial log (`pio device monitor`) for `"NodeId collision detected"` to
tell the two apart. Detection can take up to 30 seconds (the heartbeat
interval) from the moment both panels are simultaneously connected.

**Two things still work during a collision lockout, so you are never
stuck:**
- **Bench-serial commissioning** over USB — recommission the panel with
  a different node ID via `id <n>` / `save` / `reboot`.
- **The BOOT-button wireless-setup gesture** (hold 3 seconds, then
  release) — still opens the wireless setup AP even during a lockout,
  since it reads a dedicated input (GPIO0) that collision suppression
  never touches.

**Why not retained (sub-project #9b):** the topics above were originally
retained, and each panel's MQTT client ID was derived from its `nodeId` —
so two colliding panels shared a client ID, and MQTT's broker-enforced
duplicate-`clientId` behavior meant they could never be connected at the
same time (each new connection kicked the other off). Retention was the
only way the disconnected panel's last claim stayed on the broker for the
other to read on its next connect. Loco2MQTT (sub-project #9a) ignores
the retained flag entirely, so that mechanism went silently dead against
it. #9b fixed this at the root: the MQTT client ID now derives from the
panel's own MAC instead, so two colliding panels get distinct client IDs
and both stay connected simultaneously — and since both are now alive at
once and heartbeating every 30 seconds, each observes the other's live
publish without needing retention at all. **Do not revert either topic's
retained flag to `true` or revert the client ID back to a `nodeId`-derived
string — both are required together for detection to work against
Loco2MQTT.**

**Known, accepted limitation — from the original #2d-a design, no longer
applicable against Loco2MQTT:** decommissioning a panel or reassigning its
node ID used to risk one false-positive collision on the replacement's
first boot, because a stale *retained* MAC claim could sit on the broker
after the old panel stopped publishing. Since #9b's topics are no longer
retained, this exact scenario can no longer occur — there's nothing left
on the broker for the replacement to mistakenly observe once the old panel
is actually gone. See `CLAUDE.md`'s "Presence + collision detection"
section for the full current design and its remaining known limitations.

---

## Identifying a Physical Panel (identify-blink)

**Decision (2026-08-30):** publish any message to `panel/<nodeId>/identify`
to make that one physical panel's LEDs flash so you can find it among
several deployed panels — useful when verifying commissioning worked,
debugging via MQTT tooling, or telling apart two panels mid-collision
(see above; identify keeps working even during a collision lockout, since
that's exactly the situation where you need to tell two panels apart).

- **The payload is ignored** — any message triggers it, including an empty
  one.
- **All 12 LED pairs flash green/red together** for **10 seconds**, then
  stop automatically and return to normal. There is no "stop" command;
  publishing again before the 10 seconds are up just extends the window.
- **Publish this topic non-retained.** A retained message re-triggers a
  fresh 10-second flash every time the panel reconnects to the broker —
  annoying at best, and it will keep happening indefinitely since the
  panel has no way to know the retained message is stale.
- **MQTT only** — there is no bench-serial equivalent. If you already have
  a serial cable plugged into the right panel, you already know which one
  it is.
- **Turnout buttons and feedback are unaffected.** Identify is a pure
  visual overlay; it never blocks or delays normal panel operation.
- **Wireless setup mode uses this exact same flash pattern** (see the
  "Wireless Setup Access Point" section above) — the two states can never
  occur simultaneously, so seeing this flash always means one or the
  other.

Example: `mosquitto_pub -t panel/5/identify -n` (the `-n` flag sends an
empty, non-retained message).

---

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

---

## Software responsibilities

1. Connect to Wi-Fi.
2. Connect to / communicate with Loco2MQTT.
3. Scan the 3×4 button matrix.
4. Debounce button presses.
5. Detect a new press (not repeated triggers while held).
6. Map matrix position → turnout identifier.
7. Toggle / request the appropriate turnout state.
8. Send the turnout command to Loco2MQTT over MQTT.
9. Subscribe to Loco2MQTT's turnout state topic(s) and decode incoming feedback.
10. Store/retrieve the last Loco2MQTT-confirmed turnout state (not the commanded
    state — see "Loco2MQTT Communication (MQTT)" above).
11. Drive the LED GPIO: HIGH for green, LOW for red, only once Loco2MQTT confirms
    the state — never optimistically on button press.
12. Blink the last-known/default color while a turnout's state is
    unconfirmed (before first feedback, or during a connection outage).
13. Handle loss and restoration of Wi-Fi / MQTT communication, including
    resubscribing to feedback topics on reconnect.

### Suggested per-turnout config

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

### State model

```cpp
enum class TurnoutState {
    CLOSED,
    THROWN,
    UNKNOWN
};
```

- `CLOSED` → green on → GPIO HIGH
- `THROWN` → red on → GPIO LOW
- (mapping can be reversed in software to match panel convention)

**Decision (2026-07-21):** `UNKNOWN` is shown by blinking the GPIO between
HIGH/LOW at the last actively-displayed color (or a configured default color
if none has ever been displayed, e.g. at first boot). This is driven entirely
by the ESP32 LED-pair driver (Milestone 2) reacting to `TurnoutIndicator`
calling `off()` on both the thrown and closed sides (i.e. `clear()`) — see
"Connection loss and reconnection" under Loco2MQTT Communication above for
when that happens. `TurnoutState`/`UNKNOWN` here is purely an ESP32 LED-driver
concept — it does not exist in and does not need to be added to the shared
domain `TurnoutPosition` enum (`Closed`/`Thrown` only), since `TurnoutControl`
never calls `display()` until real feedback arrives.

The blink requires non-blocking timing logic in the LED-pair driver (via the
existing `Clock` port), not a plain digitalWrite — see Testing under
Milestone 2 below.

### Startup sequence

ESP32 pins can float/change behavior briefly during boot, so panel LEDs may
flash on power-up. Mitigate by configuring outputs before anything else:

1. Configure all LED GPIOs as outputs; call `clear()` on every
   `TurnoutIndicator` so all 12 LEDs start in blink/unknown mode rather than
   an undefined GPIO level.
2. Configure matrix column inputs.
3. Set up matrix scanning.
4. Connect to Wi-Fi.
5. Connect to Loco2MQTT over MQTT (resolved via mDNS, falling back to a
   manually-configured broker host); subscribe to turnout state topic(s).
6. Wait for Loco2MQTT to publish confirmed state for each turnout (each
   arrival calls that turnout's `TurnoutControl::applyFeedback()`, which
   stops that LED's blinking and shows the confirmed color) — turnouts not
   yet reported keep blinking.

---

## Physical assumptions

- Button/LED wiring runs under 1 foot.
- Only one button pressed at a time (no matrix ghosting concern).
- Standard indicator brightness is sufficient.
- Multiple ESP32 boards may be used; each manages its own group of up to 12
  turnouts.
- I/O expanders may be considered later — out of scope for this version.
- No LED multiplexing — every LED stays continuously powered rather than
  scanned.

---

## Open questions / follow-ups

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

## Suggested milestones (all shipped — kept for history)

*Section and class names referenced below are the pre-implementation
originals (e.g. "JMRI Communication (MQTT)", `JmriCommandEncoder`,
`JmriFeedbackDecoder`) — this milestone list is left as it was written and
not updated for later renames (`Loco2MQTT Communication (MQTT)` as of
sub-project #9c, `TopicScheme`/`PayloadCodec` as actually shipped). See the
current sections above for what these became.*

Mirroring the TDD-first approach used for the Mega/LocoNet system:

### Milestone 0: PlatformIO environment setup

Today `src/main.cpp` is a single composition root built by every environment
(`test_build_src = false` currently keeps it out of `native` test builds, but
there's still only one `main.cpp` for hardware targets). Adding a second
hardware target means splitting that out before any ESP32-specific code is
written:

- [ ] Add `[env:esp32dev]` to `platformio.ini`:
  ```ini
  [env:esp32dev]
  platform = espressif32
  board = esp32dev
  framework = arduino
  monitor_speed = 115200
  lib_ldf_mode = deep+
  ```
  (`esp32dev` is the generic ESP32-WROOM-32 board definition; confirm it
  matches the ELEGOO board's flash size/partition needs, adjust if not.)
- [ ] Split `src/` into per-target composition roots so each environment
  builds only its own `main.cpp` — e.g. `src/mega/main.cpp` and
  `src/esp32/main.cpp` — and set `build_src_filter` per environment
  (`+<mega/*>` for `megaatmega2560`, `+<esp32/*>` for `esp32dev`) so neither
  target tries to compile the other's hardware-specific code.
- [ ] Add an MQTT client library (e.g. `knolleary/PubSubClient` — confirm
  choice when implementing) as an `esp32dev`-only `lib_deps` entry, the same
  way `mrrwa/LocoNet` is scoped to `megaatmega2560` only.
- [ ] Confirm `lib/McsCore` domain code (no `Arduino.h` dependency) builds
  unmodified under `esp32dev` — it should, since it already builds under both
  `native` and `megaatmega2560` without changes.
- [ ] Verify `pio run -e esp32dev` and `pio run -e megaatmega2560` both still
  build cleanly after the split, and `pio test -e native` is unaffected.

### Milestone 1: Matrix scanning + debounce (native-testable)

Pure logic: row drive sequence, column read, debounce, new-press edge
detection, matrix position → turnout ID mapping. No hardware dependency, so
this can be unit tested the same way `Button` is today.

### Milestone 2: LED pair state driver (native-testable)

`TurnoutState` → GPIO level mapping, independent of actual GPIO calls,
including the blink-on-unknown behavior: both-off (`clear()`) → toggle GPIO
over simulated time (via the `Clock` port/`FakeClock`, same style as
`PulsingLocoNetTransport`'s tests) at the last actively-displayed color, or a
configured default if none has ever been displayed.

### Milestone 3: ESP32 hardware adapters

Matrix GPIO adapter, LED-pair GPIO adapter, guarded the same way
`ArduinoDigitalInput`/`Output` are for the Mega (`#ifdef ARDUINO`, living in
`lib/McsCore/src/adapters` alongside the existing ones, or a new
`esp32`-specific subfolder if the port shapes diverge enough to warrant it).

### Milestone 4: MQTT + JMRI transport (send and receive)

Connect to Wi-Fi and the MQTT broker, subscribe to turnout state topic(s),
handle disconnect/reconnect (including resubscribing). Implement
`MqttJmriTurnoutCommandAdapter` (wraps `JmriCommandEncoder`, implements
`TurnoutCommandPort`) and `MqttJmriFeedbackSource` (queues incoming messages
for `JmriFeedbackDecoder` to decode, matching `LocoNetFeedbackSource`'s
poll-shaped interface) — see JMRI Communication (MQTT) above. `JmriCommandEncoder`
and `JmriFeedbackDecoder` are native-testable; the MQTT client wrapper itself
is a hardware shim, verified on the ELEGOO board like
`MrrwaLocoNetSwitchDriver`/`MrrwaLocoNetFeedbackSource`. Blocked on the two
JMRI MQTT topic/retained-message open questions above.

### Milestone 5: Composition root (`src/esp32/main.cpp`)

Wire the config table, adapters, and 12× `TurnoutControl` together;
non-blocking `loop()` that polls the MQTT client, drains
`MqttJmriFeedbackSource` into the matching `TurnoutControl::applyFeedback()`,
and calls `clear()` on all `TurnoutIndicator`s at boot and on detected
connection loss (see "Connection loss and reconnection" above).

### Milestone 6: Hardware bring-up

One turnout end-to-end on a breadboard, then scale to all 12.
