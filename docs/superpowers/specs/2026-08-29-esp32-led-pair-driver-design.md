# ESP32 Shared-GPIO LED-Pair Driver (Sub-project #4) — Design

This is sub-project **#4** in the ESP32 panel decomposition (see
`docs/superpowers/specs/2026-08-28-esp32-node-config-commissioning-design.md`'s
sub-project table): shared-GPIO LED-pair output. It has no dependency on
2a/2b/3/6 and feeds into sub-project #5 (ESP32 hardware adapters for #3/#4)
and #7 (composition root).

## Problem

`docs/led-wiring.md` wires a red LED and a green LED to a single GPIO per
turnout (12 GPIOs for 24 LEDs instead of 24): the green LED lights when the
GPIO is driven HIGH, the red LED lights when it's driven LOW — the two are
electrically wired in opposite directions off the same pin. This means
**both LEDs off is not physically achievable** with this wiring; the GPIO
is always in one state or the other.

That's a real problem for the existing domain model. `TurnoutIndicator`
(`lib/McsCore/src/domain/TurnoutIndicator.h`, already used unmodified by
the Mega panel) has a `clear()` method that turns both its `Indicator`s
off — used today to represent "no confirmed state yet." On the Mega, with
two independent GPIOs, that's a real "both off." On the ESP32's shared-GPIO
wiring, "both off" has no direct physical meaning.

This isn't a new problem this project has to solve from scratch: an earlier
design doc (`docs/ESP32_Turnout_Panel_Implementation.md`, 2026-07-21,
predating the current sub-project split) already worked this out under
"State Model" / "Milestone 2: LED pair state driver" and made a binding
decision — see Decision below. This spec's job is to turn that decision
into this project's current class-and-port shape.

## Decision

`clear()`'s "both off" becomes **blink**: alternate the GPIO between the
last actively-displayed color and its opposite, at a configured interval,
until a real `display()` call arrives with a confirmed position. If nothing
has ever been displayed (e.g. at first boot), blink starts at a configured
default color instead. This is driven entirely by the ESP32 LED-pair driver
reacting to `TurnoutIndicator`'s *existing* `on()`/`off()` call pattern —
`TurnoutIndicator`, `Indicator`, and `TurnoutControl` are **not modified**,
matching this project's established preference (see sub-project #3) for a
new ESP32-specific adapter under an existing port over changes to shared
domain code.

Two new classes, both in `lib/McsEsp32/` — not `lib/McsCore/` — for the same
reason as sub-project #3: this is ESP32-only (the Mega has two independent
GPIOs and never needs this), and `McsCore` must stay usable by a target that
has no use for this concept at all.

- **`LedPairDriver`** (domain) owns the one real `DigitalOutput&` (the
  shared GPIO), a `Clock&`, and a blink interval passed in at construction
  — following `PulsingLocoNetTransport`'s existing precedent
  (`lib/McsLoconet/src/adapters/PulsingLocoNetTransport.h`) of taking a
  timing value as a constructor parameter rather than hardcoding it, so the
  actual millisecond value is sub-project #7's composition-root decision,
  not baked into this class. It tracks two independent boolean requests,
  `greenRequested`/`redRequested`, set via `setGreen(bool)`/`setRed(bool)`.
  Derives a mode from them on every call: both false → **blink**; exactly
  one true → **steady** that color (writing the GPIO immediately,
  synchronously — no waiting for `update()` — and remembering it as the new
  "last displayed" color for future blink fallback); both true is a
  transient no-op (see "Why both-true is safe to ignore" below). `update()`,
  called every loop, only acts in blink mode: once the configured interval
  has elapsed since the last toggle, flips the GPIO to the opposite of
  whatever it's currently showing and resets the timer. Entering blink mode
  (a mode transition, not a redundant re-entry) immediately writes the
  last-displayed-or-default color to the GPIO and starts the timer, rather
  than leaving a stale level until the first `update()` tick.
- **`LedPairOutput`** (adapter) implements the existing `DigitalOutput` port
  for one logical side (green or red) of the pair: `set(bool)` forwards to
  the driver's `setGreen`/`setRed`, `isSet()` returns the driver's tracked
  request for that side. Two instances, one per color, share one
  `LedPairDriver`. This is what lets sub-project #7 construct two ordinary
  `Indicator`s (one wrapping a green `LedPairOutput`, one wrapping a red
  one) and hand them to an ordinary, completely unmodified
  `TurnoutIndicator` — exactly the same shape the Mega already uses with
  two independent real GPIOs.

`Indicator::isOn()` (and `DigitalOutput::isSet()` generally) reflect the
*logical* request that was last made, not a live re-read of the physical
pin — this already matches `ArduinoDigitalOutput`'s existing convention
(it tracks its own `active_` field rather than reading the pin back), so
`LedPairOutput::isSet()` staying true for "red" while blink is visually
alternating the real GPIO between red and green is consistent with how
this port already behaves elsewhere in the codebase, not a special case
invented here.

### Why both-true is safe to ignore

`TurnoutIndicator::display(position)` calls one side's `on()` then the
other's `off()` in sequence (e.g. for Thrown: `thrownIndicator_.on();
closedIndicator_.off();`). If the pair was previously showing the other
color, there's a genuine intermediate instant where both
`greenRequested`/`redRequested` are true — between the two calls, never as
a final state after `display()` returns. Nothing in this codebase reads
`LedPairOutput`/`Indicator` state from a different thread or interrupt
context, so no caller can ever observe that intermediate instant. Treating
it as a no-op (leave the GPIO exactly as it was) means this transient can
never produce a visible flicker or a wrong final color, regardless of call
order.

### Color naming, not turnout-position naming

`LedPairDriver`/`LedPairOutput` speak in terms of the physical colors
(green/red), not turnout semantics (closed/thrown) — the same way
`DigitalOutput` itself has no opinion about what it's driving. Which
physical color represents "closed" vs. "thrown" for a given turnout is a
wiring/config decision `docs/led-wiring.md` already makes (green = closed =
GPIO HIGH, red = thrown = GPIO LOW, "mapping can be reversed in software to
match panel convention") — sub-project #7 makes that association when it
constructs each turnout's two `Indicator`s, exactly as it already will for
the row/column-to-turnout mapping in sub-project #3.

## Components

### `LedPairColor` (`lib/McsEsp32/src/domain/LedPairDriver.h`)

```cpp
enum class LedPairColor
{
    Green,
    Red
};
```

### `LedPairDriver` (`lib/McsEsp32/src/domain/LedPairDriver.h` / `.cpp`)

```cpp
class LedPairDriver
{
public:
    LedPairDriver(DigitalOutput& gpio, Clock& clock, unsigned long blinkIntervalMs,
                  LedPairColor defaultColor);

    void setGreen(bool active);
    void setRed(bool active);

    [[nodiscard]] bool isGreenRequested() const;
    [[nodiscard]] bool isRedRequested() const;

    void update();

private:
    // mode derivation, GPIO writes, and blink timing live here
};
```

### `LedPairOutput` (`lib/McsEsp32/src/adapters/LedPairOutput.h` / `.cpp`)

```cpp
class LedPairOutput final : public DigitalOutput
{
public:
    LedPairOutput(LedPairDriver& driver, LedPairColor color);

    void set(bool active) override;
    [[nodiscard]] bool isSet() const override;

private:
    LedPairDriver& driver_;
    LedPairColor color_;
};
```

## Testing

Fully native-testable with the existing `FakeDigitalOutput`/`FakeClock`
(`test/support/`) — no new fakes needed.

`LedPairDriver`:
- Requesting green writes the GPIO HIGH immediately (synchronously), and
  requesting red writes it LOW immediately — neither waits for `update()`.
- Requesting both off enters blink and immediately shows the last-displayed
  color (not a stale/arbitrary level) — tested both from "was showing
  green" and "was showing red" starting points.
- Before anything has ever been displayed, requesting both off immediately
  shows the configured default color.
- `update()` before the blink interval has elapsed does nothing (GPIO
  unchanged).
- `update()` after the interval has elapsed flips the GPIO to the opposite
  of whatever it's currently showing, and resets the timer for the next
  toggle.
- `update()` while *not* in blink mode (steady green or red currently
  requested) never touches the GPIO, no matter how much simulated time has
  passed.
- A redundant call that doesn't change the derived mode (e.g. requesting
  green again while already steady-green) does not reset the blink timer
  or rewrite the GPIO.
- Requesting both green and red simultaneously true (the transient
  mid-`display()` case) leaves the GPIO exactly as it was.

`LedPairOutput`:
- `set(true)`/`set(false)` on a green-side instance forwards to the
  driver's `setGreen` with the same value; a red-side instance forwards to
  `setRed`.
- `isSet()` reflects the driver's tracked request for that instance's
  color, independent of what the other color's instance most recently did.

## Non-goals

- Constructing real `ArduinoDigitalOutput` instances for the 12 real LED
  GPIOs, or wiring `Indicator`/`TurnoutIndicator`/`TurnoutControl` together
  for all 12 turnouts — that's sub-project #7's composition-root job,
  mirroring how sub-project #3 left the row/column/turnout mapping to #7.
- Choosing the actual blink interval's millisecond value, or the actual
  default color per turnout — both are `#7`'s composition-root decisions,
  passed in as constructor arguments to `LedPairDriver`.
- Sub-project #3 (matrix button scan) — unrelated, independent hardware
  concern, already complete.
- Any change to `Indicator`, `TurnoutIndicator`, `TurnoutControl`, or
  `TurnoutPosition` — all reused completely unmodified. In particular,
  `TurnoutState`/`UNKNOWN` (from the 2026-07-21 doc's sketch) is purely an
  `LedPairDriver`-internal concept; it does not need to exist in, and is
  not added to, the shared domain `TurnoutPosition` enum.
