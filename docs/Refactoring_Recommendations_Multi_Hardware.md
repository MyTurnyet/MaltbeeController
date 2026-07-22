# Refactoring Recommendations: Preparing for Multiple Hardware Environments

## Context

We're about to add a second PlatformIO environment (`esp32dev`, see
`docs/ESP32_Turnout_Panel_Implementation.md`) alongside the existing
`megaatmega2560` environment. Before writing ESP32-specific code, this
document reviews the current `lib/McsCore` and `src/main.cpp` for things that
should change now — either because they'd cause a real bug on the new
hardware, or because doing them once, generically, now saves redoing the same
work twice.

**Scope:** review only, no code changes made. Findings are ordered by
priority.

---

## Priority 1 — Do before writing ESP32 code (real bug risk)

### 1.1 `ArduinoDigitalInput` conflates "active-low" with "use internal pull-up"

**Status (2026-07-21): DONE.** `ArduinoDigitalInput` now takes `(pin, activeLow, useInternalPullup)`; `begin()` calls `pinMode(pin_, useInternalPullup_ ? INPUT_PULLUP : INPUT)` independent of polarity. Both Mega call sites in `src/main.cpp` (throw/close buttons) updated to pass `useInternalPullup=true`, matching prior behavior. No native test exists or is possible for this class (`#ifdef ARDUINO`-guarded, no `Arduino.h` under `native`); verified instead by `pio run -e megaatmega2560` building cleanly and the full native suite staying green.

`lib/McsCore/src/adapters/ArduinoDigitalInput.cpp:14`:

```cpp
void ArduinoDigitalInput::begin()
{
    pinMode(pin_, activeLow_ ? INPUT_PULLUP : INPUT);
}
```

Today there's only one caller pattern (Mega buttons wired active-low with the
AVR's internal pull-up), so `activeLow` doing double duty as "and also enable
the internal pull-up" has never mattered.

It will matter on the ESP32: the planned button-matrix columns (GPIO 34, 35,
36, 39 — see the ESP32 doc's GPIO Assignment section) are **input-only pins
with no internal pull-up hardware at all**, and the design correctly calls
for external 10 kΩ pull-ups instead. But those columns are still read as
active-low (LOW = pressed). If `ArduinoDigitalInput(pin, /*activeLow=*/true)`
is reused as-is for a matrix column, `begin()` will request `INPUT_PULLUP` on
a pin that can't provide it — not a crash, but silently wrong configuration
that only *happens* to work because the external resistor is doing the real
job. It's a landmine for the day someone reuses this constructor on a
different ESP32 pin that *does* have an internal pull-up, or ports this code
to another board.

**Recommendation:** split the two concerns before building the matrix input
adapter:

```cpp
ArduinoDigitalInput(int pin, bool activeLow, bool useInternalPullup);
```

- Mega buttons: `activeLow=true, useInternalPullup=true` (current behavior,
  unchanged).
- ESP32 matrix columns: `activeLow=true, useInternalPullup=false` — `begin()`
  calls plain `pinMode(pin_, INPUT)`.

Small, contained change (one constructor, one `begin()` body, update the two
existing call sites in `src/main.cpp`). Doing it now means the ESP32 matrix
adapter work in Milestone 3 doesn't have to touch this class under time
pressure.

---

## Priority 2 — Decide before writing `src/esp32/main.cpp` (shapes the design)

### 2.1 No existing pattern for wiring more than one turnout

**Status (2026-07-21): DECIDED, not yet implemented.** Going with **(b)** — a new `TurnoutStation`-style aggregate — over (a). Deliberately not building it yet: nothing consumes it today (`src/main.cpp` still wires exactly one turnout, and `esp32dev`/Milestone 0 hasn't started), and this project's engineering principles call for writing the minimum a current test/consumer requires rather than designing ahead for a not-yet-started milestone. Build it when Mega Milestone 11 (multiple turnouts) or ESP32 Milestone 0/5 actually starts driving construction from a config table — at that point TDD the aggregate's `bind()`/`configure()` behavior the normal way.

`Button`, `Indicator`, `TurnoutIndicator`, and `TurnoutControl` all take their
collaborators as C++ **references** in the constructor
(`lib/McsCore/src/application/TurnoutControl.h:11`,
`lib/McsCore/src/domain/Button.h:9`, etc.). `src/main.cpp` wires exactly one
turnout as a set of hand-declared global objects (lines 27–49). That's fine
for one turnout, but it doesn't extend to a loop or a config table — reference
members mean these objects aren't default-constructible, so you can't declare
`Button buttons[12];` and fill them in later; you'd have to hand-write 12
sets of the same 6+ object declarations (input, input, button, indicator ×2,
turnout, control) main.cpp currently has for one.

This isn't ESP32-specific — it's exactly the wall the Mega side will hit at
**Milestone 11 (multiple turnouts)** in the roadmap. The ESP32 panel needs it
immediately (12 turnouts on one board), so whichever pattern gets picked here
is very likely to become the pattern Milestone 11 reuses on the Mega side
too.

Contrast with `Turnout` itself (`lib/McsCore/src/domain/Turnout.h`): it's a
plain value type, default-constructible, holds no references, and is already
stored in a fixed array by `TurnoutCollection`
(`lib/McsCore/src/domain/TurnoutCollection.h:10`). That's the shape the other
classes would need to move toward to support array/loop construction.

**Recommendation:** before writing the ESP32 composition root, decide — and
this is a real design trade-off, not a mechanical fix — between:

- **(a) Keep reference-based construction, hand-unroll N stations.** Simplest
  change (none), matches current style, but means a 12-entry
  `TurnoutConfig[]`-driven design (as sketched in the ESP32 doc) can't
  actually drive construction in a loop — the 12 stations still have to be
  individually spelled out. Verbose but mechanical; low risk.
- **(b) Introduce a `TurnoutStation`-style aggregate** that owns/binds one
  turnout's worth of collaborators and can be default-constructed then
  `bind()`/`configure()`'d from a `TurnoutConfig` entry — closer to what a
  12-station, config-table-driven ESP32 panel wants, and reusable for Mega
  Milestone 11. Larger change: touches the constructor style of `Button`,
  `Indicator`, and `TurnoutControl` (or wraps them without changing them,
  using pointer members in the new aggregate instead of changing the
  existing classes).

Either is workable; the point is to choose deliberately now rather than
default into (a) by momentum and then redo it when Milestone 11 hits the same
problem on the Mega side.

---

## Priority 3 — Worth doing, not urgent

### 3.1 `lib/McsCore/src/adapters/` will get crowded

Currently a flat folder of 12 files mixing three concerns: generic Arduino
GPIO (`ArduinoClock`, `ArduinoDigitalInput`, `ArduinoDigitalOutput`),
LocoNet-specific (`Mrrwa*`, `PulsingLocoNetTransport`), and a placeholder
(`NullTurnoutCommandPort`). ESP32 work will add button-matrix, LED-pair, and
Wi-Fi/JMRI adapters on top of that. Recommend subfolders once the ESP32
adapters exist (e.g. `adapters/gpio/`, `adapters/loconet/`,
`adapters/esp32/`) purely for navigability — fine to do as part of Milestone
3 rather than as a separate pass now.

### 3.2 One GPIO representing two logical indicators (ESP32 LED pairs)

`DigitalOutput` (`lib/McsCore/src/ports/DigitalOutput.h`) models "one output
= one physical pin," and `TurnoutIndicator` already takes two independent
`Indicator&` (thrown/red, closed/green) — see
`lib/McsCore/src/domain/TurnoutIndicator.h:9`. No domain change is needed
here: the port and domain abstraction are already generic enough. The thing
to design carefully in Milestone 3 is the *adapter* — on the ESP32 one
physical GPIO drives both LEDs via complementary levels (HIGH = green,
LOW = red), so the two `DigitalOutput` instances backing "thrown indicator"
and "closed indicator" for a given turnout must share one underlying GPIO
resource and stay mutually exclusive (turning one on must be reflected as the
other going off), rather than being two independent
`ArduinoDigitalOutput(pin)` instances the way the Mega's separate-pin LEDs
are today. Flagging this now so it's not a surprise mid-Milestone-3; no
action needed before then.

---

## No action needed (confirmed fine as-is)

- **Hexagonal boundary holds up.** Every hardware touchpoint (`DigitalInput`,
  `DigitalOutput`, `Clock`, `TurnoutCommandPort`) is already behind a port,
  and the domain/application layer (`Button`, `Indicator`, `Turnout`,
  `TurnoutIndicator`, `TurnoutControl`, `TurnoutCollection`, `Route`,
  `RouteService`) has zero Arduino dependency. This is exactly what makes an
  ESP32 environment additive rather than a rewrite — expect to reuse all of
  `lib/McsCore/src/domain` and `lib/McsCore/src/application` unchanged.
- **`platformio.ini` already scopes hardware-specific libraries per
  environment** — `lib_deps = https://github.com/mrrwa/LocoNet.git#1.1.13` is
  declared only under `[env:megaatmega2560]`. This is the exact pattern to
  follow for the ESP32's Wi-Fi/JMRI client library under `[env:esp32dev]`
  (already captured as a task in the ESP32 doc's Milestone 0 — noted here
  only to confirm there's a working precedent, not a new finding).
- **Fixed-capacity domain types (`FixedString32`, `TurnoutCollection`'s
  `Turnout[64]`, `RouteService`'s `Route[32]`) don't need per-platform
  tuning.** These limits exist because of the Mega's 8 KB RAM (see
  CLAUDE.md's AVR/STL section), and the ESP32 has far more headroom, but
  sharing one conservative capacity across both targets is simpler than
  maintaining platform-specific limits and no real panel is likely to
  approach 64 turnouts on a single board anyway. Leave as-is.

---

## Suggested order of work

1. ✅ Fix 1.1 (`ArduinoDigitalInput` pull-up/polarity split) — small, isolated,
   removes a latent bug before matrix code depends on it. Done 2026-07-21.
2. ✅ Decide 2.1 (multi-station composition pattern) — this is a design
   decision, not code; make it before Milestone 0/3 implementation starts
   since it affects the shape of the config table and composition root.
   Decided 2026-07-21 (option b, `TurnoutStation` aggregate); implementation
   deliberately deferred to whichever of Mega Milestone 11 or ESP32
   Milestone 0/5 starts first.
3. Proceed with `docs/ESP32_Turnout_Panel_Implementation.md` Milestone 0
   (PlatformIO environment split) with 1.1 and 2.1 settled.
4. Address 3.1 and 3.2 inline during Milestone 3 (ESP32 hardware adapters) —
   no separate pass needed.
