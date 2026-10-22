# 4-Turnout Node: Independent-Output Turnout Indicator (Sub-project #2) — Design

This is sub-project **#2** of the 4-turnout node board effort (see
`docs/superpowers/specs/2026-10-21-esp32-node4-channel-count-design.md` for
sub-project #1, the channel-count templating this one builds on top of).
It depends on nothing from #1 directly — it's independent domain/adapter
logic — and feeds into whichever later sub-project wires turnout stations
into `src/esp32node4/main.cpp`.

## Problem

`docs/pcb/turnout-node-4ch-pcb-design.md` §2.3 gives each turnout's red and
green LED its own GPIO, instead of the 12-channel panel's single shared pin
(HIGH = green, LOW = red). That means, for the first time on this project's
ESP32 targets, **both off and both on (amber) are physically real states**
— the 12-channel panel's `LedPairDriver` exists specifically because a
shared pin *can't* go dark, so `TurnoutIndicator::clear()`'s "both off"
call had to become a blink instead. On the node board that constraint is
gone, so "what does clear() show now?" is a genuine open design question,
not a forced consequence of the wiring.

Discussed and decided with Paige: keep the exact visual the 12-channel
panel already has — alternating red/green — rather than introducing amber
or a fully-dark unconfirmed state. Two reasons: it's the behavior anyone
already familiar with the existing panel will expect, and introducing
amber for "unconfirmed" would spend the one genuinely new piece of visual
vocabulary (true off, true amber) on a state that doesn't need it, leaving
nothing distinctive left over for an actual future "inconsistent feedback"
signal if one is ever needed. Amber and true-off stay unused by this
indicator for now.

## Decision

A new driver class, independent of the existing `LedPairDriver`, drives two
real `DigitalOutput&`s (green, red) with the **same mode/blink state
machine** `LedPairDriver` already has — same `LedPairColor` enum
(reused, not duplicated), same derived modes (both-requested-false →
blink; exactly one true → steady that color; both true → transient no-op),
same "blink alternates between the last-displayed color and its opposite"
algorithm, same `setIdentifying()` override. The only thing that differs is
`writeColor()`: instead of writing one shared GPIO HIGH/LOW, it writes
*both* real GPIOs so that exactly one is ever lit at a time — including
during blink, which is what makes the two drivers' outward behavior
identical despite one physically being able to go dark and the other not.

**Not shared with `LedPairDriver` via inheritance or an extracted helper.**
Two independent classes, deliberately duplicating the mode/blink logic,
decided explicitly with Paige over extracting a shared base: the two
drivers have genuinely different physical constraints that only happen to
produce the same *behavior* because of the choice above, not because
they're structurally the same thing — a future redesign of either board's
indicator (e.g. the node board eventually using amber for something) could
diverge them again, and refactoring the already-shipped, tested
`LedPairDriver` now to share logic it doesn't yet need to share would be
premature coupling for a one-time ~90-line duplication. `LedPairDriver`
and `LedPairOutput` are **not modified** by this sub-project.

Both new classes live in `lib/McsEsp32/`, matching where `LedPairDriver`/
`LedPairOutput` already live — this is ESP32-only, and `McsCore` has no use
for either shared- or independent-GPIO LED pairs.

- **`IndependentLedPairDriver`** (domain) — constructor takes
  `DigitalOutput& greenGpio, DigitalOutput& redGpio, Clock& clock,
  unsigned long blinkIntervalMs, LedPairColor defaultColor`. Public
  interface otherwise matches `LedPairDriver` exactly: `begin()`,
  `setGreen(bool)`, `setRed(bool)`, `isGreenRequested()`,
  `isRedRequested()`, `setIdentifying(bool)`, `update()`. Internally,
  `writeColor(color)` sets `greenGpio_.set(color == Green)` and
  `redGpio_.set(color == Red)` together, so green and red are always
  mutually exclusive — blink shows one color at a time by writing one GPIO
  true and the other false, then swapping which is which, never both true
  at once. `setIdentifying()` is included even though nothing wires it to
  be called yet (that's a later sub-project's job, mirroring
  `LedPairFlashIndicator`/the `panel/<nodeId>/identify` overlay for
  independent outputs) — leaving it out would make this a partial port of
  the state machine rather than a complete one, for no benefit, since it
  costs nothing extra and no wiring elsewhere is required to add it now.
- **`IndependentLedPairOutput`** (adapter) implements `DigitalOutput` for
  one logical side (green or red): `set(bool)` forwards to the driver's
  `setGreen`/`setRed`, `isSet()` returns the driver's tracked request for
  that side. Two instances share one `IndependentLedPairDriver`, exactly
  mirroring how two `LedPairOutput`s share one `LedPairDriver` today.

### Scope boundary: no GPIO-owning composition helper yet

Unlike `LedPairDriver`/`LedPairOutput`, which got a `LedPairStation`
composition helper (owns the real GPIO, the driver, and both outputs) in
the same sub-project that introduced them, this sub-project stops at the
two translation-layer classes. Decided explicitly with Paige: the original
task breakdown scoped this sub-project as "an adapter," and a GPIO-owning
helper has no logic of its own to design or test (it's pure construction
wiring) — it gets added in whichever later sub-project actually wires
turnout stations into `src/esp32node4/main.cpp`, alongside the direct-GPIO
buttons and composite setup trigger that sub-project also needs.

## Components

### `IndependentLedPairDriver` (`lib/McsEsp32/src/domain/IndependentLedPairDriver.h` / `.cpp`)

```cpp
#include "LedPairDriver.h" // for LedPairColor — reused, not duplicated

class IndependentLedPairDriver
{
public:
    IndependentLedPairDriver(DigitalOutput& greenGpio, DigitalOutput& redGpio, Clock& clock,
                              unsigned long blinkIntervalMs, LedPairColor defaultColor);

    void begin();

    void setGreen(bool active);
    void setRed(bool active);

    [[nodiscard]] bool isGreenRequested() const;
    [[nodiscard]] bool isRedRequested() const;

    void setIdentifying(bool active);

    void update();

private:
    // Same Mode enum, mode derivation, blink timing, and identify-override
    // logic as LedPairDriver — writeColor() is the only part that differs,
    // writing both greenGpio_ and redGpio_ instead of one shared pin.
};
```

### `IndependentLedPairOutput` (`lib/McsEsp32/src/adapters/IndependentLedPairOutput.h` / `.cpp`)

```cpp
class IndependentLedPairOutput final : public DigitalOutput
{
public:
    IndependentLedPairOutput(IndependentLedPairDriver& driver, LedPairColor color);

    void set(bool active) override;
    [[nodiscard]] bool isSet() const override;

private:
    IndependentLedPairDriver& driver_;
    LedPairColor color_;
};
```

## Testing

Fully native-testable with the existing `FakeDigitalOutput`/`FakeClock`
(`test/support/`) — no new fakes needed, same as `LedPairDriver`'s own
tests.

`IndependentLedPairDriver` (mirrors `test_led_pair_driver`'s cases, adapted
for two GPIOs instead of one):
- Requesting green writes the green GPIO HIGH and the red GPIO LOW
  immediately (synchronously); requesting red does the reverse — neither
  waits for `update()`.
- Requesting both off enters blink and immediately shows the
  last-displayed color (one GPIO true, the other false) — tested both from
  "was showing green" and "was showing red" starting points.
- Before anything has ever been displayed, requesting both off immediately
  shows the configured default color.
- `update()` before the blink interval has elapsed does nothing (both
  GPIOs unchanged).
- `update()` after the interval has elapsed flips which GPIO is true and
  which is false, and resets the timer.
- `update()` while *not* in blink mode never touches either GPIO.
- A redundant call that doesn't change the derived mode does not reset the
  blink timer or rewrite either GPIO.
- Requesting both green and red simultaneously true (the transient
  mid-`display()` case) leaves both GPIOs exactly as they were.
- `isGreenRequested`/`isRedRequested` reflect the last request for each
  side independently.
- `begin()` re-writes the current color to both GPIOs and re-stamps the
  blink timer.
- `setIdentifying(true)` immediately shows green on both GPIOs (green
  true, red false) regardless of current mode; `update()` toggles the
  identify flash at its own fixed interval, independent of the blink
  interval; a redundant `setIdentifying(true)` does not reset the toggle
  timer; `setIdentifying(false)` reverts to whatever the normal mode would
  show (steady or blink's current color), not a guess; `update()` does not
  run normal blink logic while identifying.

`IndependentLedPairOutput`:
- `set(true)`/`set(false)` on a green-side instance forwards to the
  driver's `setGreen` with the same value; a red-side instance forwards to
  `setRed`.
- `isSet()` reflects the driver's tracked request for that instance's
  color, independent of what the other color's instance most recently did.

## Non-goals

- Constructing real `ArduinoDigitalOutput` instances for the node board's 8
  real LED GPIOs, or a `IndependentLedPairStation`-style composition
  helper — deferred to whichever sub-project wires turnout stations into
  `src/esp32node4/main.cpp` (see "Scope boundary" above).
- Choosing the actual blink interval or default color per turnout — both
  remain composition-root decisions, passed in as constructor arguments,
  exactly as for `LedPairDriver`.
- Any change to `LedPairDriver`, `LedPairOutput`, `Indicator`,
  `TurnoutIndicator`, `TurnoutControl`, or `TurnoutPosition` — all reused
  completely unmodified, on both the panel and the node board.
- Identify-overlay *wiring* (subscribing to `panel/<nodeId>/identify`,
  calling `setIdentifying()` from `main.cpp`) — the driver exposes the
  method per "Decision" above, but nothing calls it yet; that's a later
  sub-project's job once the node board has MQTT wiring at all.
- Status/activity LEDs, direct-GPIO buttons, the composite setup trigger —
  unrelated hardware concerns, later sub-projects.
