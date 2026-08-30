# ESP32 Turnout Panel — Implementation Tracking

## Status

**Planning.** No code written yet. This document captures the hardware design and
software plan worked out in a ChatGPT conversation (2026-07-21) so it can guide
implementation and be revised as decisions change. The JMRI communication design
(MQTT transport, bidirectional send/feedback, connection-loss handling) was
refined in a follow-up Claude Code session (2026-07-21) — see "JMRI Communication
(MQTT)" below.

## Relationship to the Mega/LocoNet system

This is a **separate hardware platform** from the Mega 2560 / LocoNet system
described in `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`:

| | Mega 2560 panel (existing) | ESP32 panel (this doc) |
|---|---|---|
| MCU | Arduino Mega 2560 | ESP32-WROOM-32 (ELEGOO dev board) |
| Turnout command transport | LocoNet (direct to DR5000) | Wi-Fi (MQTT) to JMRI |
| Input wiring | Discrete pins per button | 3×4 matrix (7 pins for 12 buttons) |
| LED wiring | Discrete pins per indicator LED | 1 GPIO per red/green pair (12 pins for 24 LEDs) |
| Feedback | LocoNet feedback (Milestone 10) | JMRI-confirmed state via MQTT (this doc) — LEDs reflect JMRI's reported state, not just what was commanded |

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
- An MQTT command sent to JMRI when the button is pressed
- LEDs that show JMRI's **confirmed** state for that turnout — updated only once
  JMRI publishes the resulting state back over MQTT (not optimistically on
  button press), and updated the same way when the turnout changes for any
  other reason (another panel, PanelPro, a dispatcher) — see "JMRI
  Communication (MQTT)" below

Design goal: maximize turnout controls per ESP32 while keeping boot reliable and
USB available for programming/debugging.

---

## Hardware

### Controller

ELEGOO ESP32 Development Board:
- ESP32-WROOM-32, USB-C, CP2102 USB-to-serial
- Wi-Fi used continuously to talk to JMRI
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

### Pins intentionally avoided

- GPIO 1 / 3 — USB serial TX/RX
- GPIO 6–11 — connected to flash memory
- GPIO 0, 2, 5, 12, 15 — boot-strapping pins

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
  button input, LED indication, and Wi-Fi/JMRI communication.

---

## Wireless Setup Access Point

**Decision (2026-08-30):** commissioning over Wi-Fi (sub-project #2c) opens a
temporary access point named `MaltBee-Setup-XXXX` (last 4 hex digits of the
ESP32's MAC address) whenever the panel boots into wireless setup mode.
Wireless setup mode is entered by holding turnout buttons T1+T2 together for
3 seconds — this works the same way whether the panel is a factory-fresh
board that has never been configured, or an already-commissioned panel a
technician wants to reconfigure. A factory-fresh panel does **not**
automatically open the AP on its own; without the T1+T2 hold it boots
waiting for bench-serial commissioning instead (see "JMRI Communication
(MQTT)" and the bench-serial commissioning design for that path).

The AP requires a WPA2 passphrase to join: **`maltbee-setup`**. This is
fixed and shared across every panel (not per-panel or MAC-derived) — write
it down for field commissioning, since a technician without it cannot join
the AP to reach the setup web form.

The panel's own current WiFi password is never displayed by the setup
form — a blank password field on submission keeps the existing password
unchanged. Turnout JMRI name fields work the opposite way: a blank field on
submission clears that turnout's assigned name.

---

## Multi-Panel Presence and Node ID Collisions

**Decision (2026-08-30):** every panel publishes two MQTT topics on
connect — `panel/<nodeId>/status` (`"online"`/`"offline"`) and
`panel/<nodeId>/mac` (the panel's own MAC, last 4 hex digits) — so a
technician with any MQTT client can see which panels are up and which
physical panel currently claims a given node ID.

**If two panels are ever accidentally commissioned with the same node
ID**, each one detects the other by watching its own `mac` topic: seeing
a MAC that isn't its own means a second panel claims this ID. When that
happens, **the panel goes visibly unresponsive** — all 12 turnout buttons
stop working and every LED falls back to the same blinking
"unconfirmed/disconnected" state already used when MQTT is down. This
looks identical to a lost network connection at a glance; check the
serial log (`pio device monitor`) for `"NodeId collision detected"` to
tell the two apart.

**Two things still work during a collision lockout, so you are never
stuck:**
- **Bench-serial commissioning** over USB — recommission the panel with
  a different node ID via `id <n>` / `save` / `reboot`.
- **The T1+T2 wireless-setup combo** (hold both buttons 3 seconds) — still
  opens the wireless setup AP even during a lockout, since it reads the
  same underlying buttons before the lockout's suppression is applied.

**If you decommission a panel or reassign its node ID to a different
physical board**, the new panel's *first* boot may briefly show a false
collision — the old panel's MAC is still sitting in the retained
`panel/<nodeId>/mac` topic from before. This clears itself automatically:
the new panel's own presence announcement overwrites the stale claim on
that same boot, so a second boot (or the automatic reconnect that follows
a dropped WiFi/MQTT session) comes up clean. No broker administration or
manual topic-clearing is ever required.

---

## JMRI Communication (MQTT)

**Decision (2026-07-21):** the ESP32 talks to JMRI over MQTT, both to send
turnout commands and to receive turnout state-change notifications. MQTT was
chosen over JMRI's WebSocket JSON server, the WiThrottle/simple TCP protocol,
and HTTP polling because it gives a persistent, push-based connection for
feedback (no polling) with a comparatively simple client library on the ESP32
side.

This reuses the existing Mega/LocoNet application layer almost unchanged —
`TurnoutCommandPort`, `TurnoutControl`, and `TurnoutFeedback`
(`lib/McsCore/src/ports/TurnoutCommandPort.h`,
`lib/McsCore/src/application/TurnoutControl.h`) are already transport-agnostic.
The ESP32-specific work is new adapters that plug into those same interfaces,
mirroring the pattern already used for LocoNet
(`MrrwaLocoNetTurnoutAdapter` / `LocoNetFeedbackDecoder`):

- **`MqttJmriTurnoutCommandAdapter`** implements `TurnoutCommandPort::send()`.
  Looks up the turnout's `jmriSystemName` from the config table and publishes
  the command over MQTT. The topic/payload construction is factored into a
  pure, native-testable `JmriCommandEncoder` so only the actual
  `mqttClient.publish()` call is untestable off-hardware.
- **`MqttJmriFeedbackSource`** — a poll-shaped port matching
  `LocoNetFeedbackSource::poll()`. The MQTT client library's subscribe
  callback pushes incoming `{topic, payload}` messages into a small bounded
  queue; `poll()` drains one per call, non-blocking.
- **`JmriFeedbackDecoder::decode()`** — native-testable, mirrors
  `LocoNetFeedbackDecoder`. Matches an incoming topic's system name against
  the config table to recover the numeric `address`, and maps the payload
  string to `TurnoutPosition`, producing a `TurnoutFeedbackLookup` exactly
  like the LocoNet decoder does.
- **12× `TurnoutControl`**, unmodified, one per turnout, each wired to its
  button pair, LED-pair indicator, and the shared
  `MqttJmriTurnoutCommandAdapter`.

### Data flow

**Command (panel → JMRI):** button press → `TurnoutControl::update()` (already
existing, unmodified) → `turnoutCommandPort_.send(address, position)` →
`MqttJmriTurnoutCommandAdapter` publishes over MQTT. `update()` never touches
the indicator, so the LED does not change at this point.

**Feedback (JMRI → panel):** JMRI publishes the turnout's new state on MQTT —
whether that's because of this panel's command or because it changed for any
other reason. The ESP32 (subscribed to turnout state topics, one wildcard
subscription covering all 12 turnouts is simplest) queues the message via
`MqttJmriFeedbackSource`; each `loop()` the composition root drains the queue,
decodes each entry with `JmriFeedbackDecoder`, and on a match calls that
turnout's `TurnoutControl::applyFeedback(feedback)` — which is the only place
`indicator_.display()` gets called. Because `TurnoutControl` is reused
unmodified, "wait for MQTT confirmation before updating the LED" and "reflect
changes made elsewhere" both fall out of the existing class for free — no new
domain/application logic needed.

### Connection loss and reconnection

`TurnoutIndicator::clear()` (`thrownIndicator_.off(); closedIndicator_.off();`
— already exists, currently unused by `TurnoutControl`) is the hook for "no
confirmed state." The ESP32 LED-pair driver (Milestone 2/3) treats "both off"
as blink mode, showing whichever color it last actively displayed (or a
configured default if it has never displayed anything, e.g. at first boot) —
see State Model below.

- **Startup:** before connecting to Wi-Fi/MQTT, the composition root calls
  `clear()` on all 12 `TurnoutIndicator`s, so the panel boots with every LED
  blinking a default color rather than an undefined GPIO level.
- **On disconnect:** when the composition root detects the MQTT connection has
  dropped, it calls `clear()` on all 12 `TurnoutIndicator`s — every LED starts
  blinking its last-known color, signaling "not currently confirmed."
- **On reconnect:** the ESP32 resubscribes to the feedback topic(s); LEDs stay
  in blink mode until each turnout's state is republished by JMRI.

**Open question:** whether JMRI's MQTT connection publishes turnout state as a
*retained* message (so a freshly (re)subscribed client gets current state
immediately) or only publishes on change (meaning a panel could stay in
blink/stale state indefinitely after a reconnect, until someone actually
operates that turnout again). Needs verification against JMRI's MQTT
documentation before implementation — see Open Questions below.

---

## Software responsibilities

1. Connect to Wi-Fi.
2. Connect to / communicate with the JMRI server.
3. Scan the 3×4 button matrix.
4. Debounce button presses.
5. Detect a new press (not repeated triggers while held).
6. Map matrix position → turnout identifier.
7. Toggle / request the appropriate turnout state.
8. Send the turnout command to JMRI over MQTT.
9. Subscribe to JMRI's turnout state topic(s) and decode incoming feedback.
10. Store/retrieve the last JMRI-confirmed turnout state (not the commanded
    state — see "JMRI Communication (MQTT)" above).
11. Drive the LED GPIO: HIGH for green, LOW for red, only once JMRI confirms
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
    const char* jmriSystemName;
};

TurnoutConfig turnouts[] = {
    {1, 0, 0, 4,  "LT1"},
    {2, 0, 1, 13, "LT2"},
    {3, 0, 2, 14, "LT3"},
    {4, 0, 3, 16, "LT4"},
    {5, 1, 0, 17, "LT5"},
    {6, 1, 1, 22, "LT6"},
    {7, 1, 2, 23, "LT7"},
    {8, 1, 3, 25, "LT8"},
    {9, 2, 0, 26, "LT9"},
    {10, 2, 1, 27, "LT10"},
    {11, 2, 2, 32, "LT11"},
    {12, 2, 3, 33, "LT12"}
};
```

`jmriSystemName` values above are placeholders — must be set to match the real
layout's JMRI turnout names before deployment. `jmriSystemName` is also the
key used to derive both the MQTT command topic and the state/feedback topic
for that turnout (exact topic scheme still TBD — see Open Questions) — no
separate topic fields are needed in this struct.

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
"Connection loss and reconnection" under JMRI Communication above for when
that happens. `TurnoutState`/`UNKNOWN` here is purely an ESP32 LED-driver
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
5. Connect to JMRI over MQTT; subscribe to turnout state topic(s).
6. Wait for JMRI to publish confirmed state for each turnout (each arrival
   calls that turnout's `TurnoutControl::applyFeedback()`, which stops that
   LED's blinking and shows the confirmed color) — turnouts JMRI hasn't
   reported yet keep blinking.

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

- [x] JMRI communication protocol/transport — decided 2026-07-21: MQTT (see
      JMRI Communication (MQTT) above).
- [x] `UNKNOWN`-state handling — decided 2026-07-21: blink last-known/default
      color (see State Model above).
- [ ] MQTT broker: not yet confirmed whether one already exists on the layout
      network, or needs to be stood up (e.g. Mosquitto) and JMRI's MQTT
      system connection configured to point at it.
- [ ] Exact JMRI MQTT topic structure and payload format for turnout commands
      and state (needed before `JmriCommandEncoder`/`JmriFeedbackDecoder` can
      be implemented) — confirm against JMRI's MQTT connection documentation.
- [ ] Whether JMRI's MQTT connection publishes turnout state as a retained
      message (immediate state on fresh subscribe) or only on change (affects
      how long a panel stays in blink/stale state after reconnecting).
- [ ] Real JMRI system names for each turnout (placeholders above are
      `LT1`–`LT12`).
- [ ] Verify GPIO 4 boot behavior on the actual ELEGOO board before wiring
      turnout 12's LEDs to it.
- [ ] Confirm ESP32 board power behavior before ruling out external 5V/VIN.

## Suggested milestones (draft — not yet started)

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
