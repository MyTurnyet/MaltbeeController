# ESP32 Turnout Panel — Implementation Tracking

## Status

**Planning.** No code written yet. This document captures the hardware design and
software plan worked out in a ChatGPT conversation (2026-07-21) so it can guide
implementation and be revised as decisions change.

## Relationship to the Mega/LocoNet system

This is a **separate hardware platform** from the Mega 2560 / LocoNet system
described in `internal_documents/MaltBee_Control_System_Architecture_and_Roadmap.md`:

| | Mega 2560 panel (existing) | ESP32 panel (this doc) |
|---|---|---|
| MCU | Arduino Mega 2560 | ESP32-WROOM-32 (ELEGOO dev board) |
| Turnout command transport | LocoNet (direct to DR5000) | Wi-Fi to JMRI |
| Input wiring | Discrete pins per button | 3×4 matrix (7 pins for 12 buttons) |
| LED wiring | Discrete pins per indicator LED | 1 GPIO per red/green pair (12 pins for 24 LEDs) |
| Feedback | LocoNet feedback (Milestone 10) | None yet — commanded state only |

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
- A Wi-Fi command sent to JMRI when the button is pressed
- LEDs that show the **commanded** turnout state, not physical position feedback

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

## Software responsibilities

1. Connect to Wi-Fi.
2. Connect to / communicate with the JMRI server.
3. Scan the 3×4 button matrix.
4. Debounce button presses.
5. Detect a new press (not repeated triggers while held).
6. Map matrix position → turnout identifier.
7. Toggle / request the appropriate turnout state.
8. Send the turnout command to JMRI.
9. Store/retrieve the commanded turnout state.
10. Drive the LED GPIO: HIGH for green, LOW for red.
11. Restore LED states after startup or reconnection.
12. Handle loss and restoration of Wi-Fi / JMRI communication.

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
layout's JMRI turnout names before deployment.

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

`UNKNOWN` has no direct one-GPIO representation. Options to revisit later:
- Default to red (fail-safe) or green on unknown
- Blink the GPIO as a warning
- Move to a two-GPIO LED design
- Add external LED-driver hardware

For now: every LED pair always shows red or green, never both off.

### Startup sequence

ESP32 pins can float/change behavior briefly during boot, so panel LEDs may
flash on power-up. Mitigate by configuring outputs before anything else:

1. Configure all LED GPIOs as outputs; set every pair to a defined initial
   state.
2. Configure matrix column inputs.
3. Set up matrix scanning.
4. Connect to Wi-Fi.
5. Connect to JMRI.
6. Retrieve/restore commanded turnout states.
7. Update all LEDs to match.

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

- [ ] JMRI communication protocol/transport: REST (WiThrottle/JSON servlet),
      raw socket, MQTT? Needs a decision before a `JmriCommandPort`-equivalent
      can be designed.
- [ ] Real JMRI system names for each turnout (placeholders above are
      `LT1`–`LT12`).
- [ ] Verify GPIO 4 boot behavior on the actual ELEGOO board before wiring
      turnout 12's LEDs to it.
- [ ] Decide `UNKNOWN`-state handling (see State Model above).
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
- [ ] Add the Wi-Fi/HTTP (or MQTT, pending the JMRI transport decision) client
  library as an `esp32dev`-only `lib_deps` entry, the same way
  `mrrwa/LocoNet` is scoped to `megaatmega2560` only.
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

`TurnoutState` → GPIO level mapping, independent of actual GPIO calls.

### Milestone 3: ESP32 hardware adapters

Matrix GPIO adapter, LED-pair GPIO adapter, guarded the same way
`ArduinoDigitalInput`/`Output` are for the Mega (`#ifdef ARDUINO`, living in
`lib/McsCore/src/adapters` alongside the existing ones, or a new
`esp32`-specific subfolder if the port shapes diverge enough to warrant it).

### Milestone 4: Wi-Fi + JMRI transport

Connect, send command, handle disconnect/reconnect. Blocked on the JMRI
protocol/transport decision in Open Questions above.

### Milestone 5: Composition root (`src/esp32/main.cpp`)

Wire config table, adapters, and application logic together; non-blocking
`loop()`.

### Milestone 6: Hardware bring-up

One turnout end-to-end on a breadboard, then scale to all 12.
