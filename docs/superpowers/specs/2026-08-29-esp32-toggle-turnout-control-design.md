# ESP32 Single-Button Toggle Turnout Control (Sub-project #7a) — Design

This is sub-project **#7a**, split out of sub-project #7 (Composition root)
in the ESP32 panel decomposition (see
`docs/superpowers/specs/2026-08-28-esp32-node-config-commissioning-design.md`'s
sub-project table). Sub-project #7's original scope — "wire everything
together into `src/esp32/main.cpp`" — turned out to hide a piece of
genuinely new domain/application logic, not just composition: this spec
covers that piece. The remaining pure-wiring work stays sub-project #7b.

## Problem

`TurnoutControl` (`lib/McsCore/src/application/TurnoutControl.h`) requires
two independent `Button&` references — `throwButton` and `closeButton` —
each edge-detected separately:

```cpp
void TurnoutControl::update()
{
    if (throwButton_.wasPressed())
    {
        turnoutCommandPort_.send(turnout_.address(), TurnoutPosition::Thrown);
    }
    if (closeButton_.wasPressed())
    {
        turnoutCommandPort_.send(turnout_.address(), TurnoutPosition::Closed);
    }
}
```

This matches the Mega panel's wiring (two physical buttons per turnout,
`TurnoutConfig::throwButtonPin`/`closeButtonPin` in
`lib/McsCore/src/adapters/TurnoutStation.h`). The ESP32 panel has exactly
**one** button per turnout — a single cell in the 3×4 matrix (sub-project
#3's `MatrixDigitalInput`). Wiring the same `Button` object into both
`throwButton`/`closeButton` slots would fire **both** `if` blocks on a
single press, sending both `Thrown` and `Closed` commands back to back —
confirmed by reading `TurnoutControl::update()` directly, not assumed.

`Turnout` already has a `toggle()`/`canThrow()` method pair
(`lib/McsCore/src/domain/Turnout.h`), but grepping the codebase confirms
they're used only by `TurnoutService` (an unrelated, unused-by-this-panel
higher-level abstraction), never by `TurnoutControl` or any
`TurnoutCommandPort` send path. No existing code solves this.

Sub-project #3's own design spec explicitly flagged this as unsolved:
*"Toggle-vs-throw/close command semantics for what a matrix button press
should send — deferred to whichever sub-project designs the ESP32's
single-button turnout-control variant (#6-adjacent or #7)."* Sub-project #6
didn't address it either (it only fixed JMRI/MQTT topic self-echo).

## Decision

A new, standalone class, `ToggleTurnoutControl`, in
`lib/McsEsp32/src/application/` — ESP32-only, since the Mega's two-button
wiring never needs this. **No changes to `TurnoutControl`**, which stays
exactly as it is today (hardware-verified on the Mega side, per
`CLAUDE.md`'s Milestone 8-11 status).

```cpp
class ToggleTurnoutControl
{
public:
    ToggleTurnoutControl(Button& button, Turnout& turnout, TurnoutIndicator& indicator,
                          TurnoutCommandPort& turnoutCommandPort);

    void update();
    void applyFeedback(TurnoutFeedback feedback);

private:
    Button& button_;
    Turnout& turnout_;
    TurnoutIndicator& indicator_;
    TurnoutCommandPort& turnoutCommandPort_;
};
```

- `update()`: if `button_.wasPressed()`, compute the opposite of
  `turnout_.position()` and call `turnoutCommandPort_.send(turnout_.address(),
  opposite)`. `Turnout::position()` reflects the last JMRI-confirmed state
  (or the position it was constructed with, before any feedback has ever
  arrived) — **not** a separately-tracked "last commanded" value. Nothing
  else in this codebase tracks a commanded-but-unconfirmed intent, and
  toggling against the confirmed position keeps this class's state
  identical in shape to `TurnoutControl`'s (none beyond the references it
  holds). A consequence, deliberately accepted: repeated presses before any
  confirmation arrives resend the *same* opposite command every time — this
  is idempotent (matches how the topic self-echo design doc already notes
  "re-receiving its own just-published state is an idempotent move to the
  position you're already at") and consistent with this project's "never
  optimistically update local state on send" rule, since nothing local
  changes as a result of pressing.
- `applyFeedback()`: behaviorally identical to
  `TurnoutControl::applyFeedback()` — filters by address, updates
  `turnout_`'s position via `throwDiverging()`/`throwStraight()`, and calls
  `indicator_.display(feedback.position)`. This ~15-line duplication is
  deliberate, not an oversight: sharing it would require either extracting
  a base/interface that touches the existing, hardware-verified
  `TurnoutControl`, or coupling two classes that should be free to evolve
  independently, for a small block of logic. Matches this project's
  established preference (three similar lines beat a premature
  abstraction) over introducing shared inheritance/interfaces for this.

## Components

### `ToggleTurnoutControl` (`lib/McsEsp32/src/application/ToggleTurnoutControl.h` / `.cpp`)

Shown in full above. Constructor stores all four references, matching
`TurnoutControl`'s existing constructor-injection style exactly (minus the
second button).

## Testing

Fully native-testable with existing test doubles — no new fakes needed:
`FakeDigitalInput`, `FakeClock` (for `Button`), `FakeDigitalOutput` (for
`Indicator`), `FakeTurnoutCommandPort` (`test/support/`).

- A press sends the opposite of the turnout's current (constructed-default)
  position.
- After `applyFeedback()` confirms a position, the next press sends the
  opposite of *that* confirmed position, not the original default.
- Repeated presses with no intervening feedback send the identical command
  each time (not an alternating toggle) — proves the "toggle against
  confirmed position, not a separate commanded-intent flag" decision above.
- `applyFeedback()` for a non-matching address leaves both the turnout's
  position and the indicator's displayed state untouched.
- `applyFeedback()` for a matching address updates both the turnout's
  position and the indicator's displayed color, for both positions
  (`Closed`→`Thrown` and `Thrown`→`Closed`).
- Holding the button (no new press edge on a subsequent `update()`) sends
  nothing.

## Non-goals

- `Turnout::isLocked()`/`isDisabled()` checks — out of scope. The existing
  `TurnoutControl` doesn't check these either in its `update()`, so this
  class stays at parity with that behavior rather than fixing an unrelated,
  pre-existing gap.
- Wiring `ToggleTurnoutControl` into `src/esp32/main.cpp`, constructing the
  12 real `MatrixDigitalInput`/`Button`/`Turnout`/`TurnoutIndicator`/
  `ToggleTurnoutControl` stacks, the row/column-to-turnout mapping, the
  green/red-to-closed/thrown color mapping, `blinkIntervalMs`/`defaultColor`
  values, NodeConfig/commissioning boot wiring, or the WiFi/MQTT connection
  lifecycle — all sub-project #7b.
- Any change to `TurnoutControl`, `Button`, `Turnout`, `TurnoutIndicator`,
  or `TurnoutCommandPort` — all reused completely unmodified.
