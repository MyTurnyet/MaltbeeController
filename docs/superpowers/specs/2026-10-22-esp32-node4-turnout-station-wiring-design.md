# 4-Turnout Node: Turnout Station Wiring (Sub-project #4) — Design

This is sub-project **#4** of the 4-turnout node board effort. It closes
out two gaps the earlier sub-projects deliberately left open:

- Sub-project #2
  (`docs/superpowers/specs/2026-10-21-esp32-node4-independent-led-pair-driver-design.md`)
  built `IndependentLedPairDriver`/`IndependentLedPairOutput` but
  explicitly deferred "a GPIO-owning composition helper... to whichever
  later sub-project wires turnout stations into
  `src/esp32node4/main.cpp`."
- Sub-project #3
  (`docs/superpowers/specs/2026-10-21-esp32-node4-setup-trigger-wiring-design.md`)
  narrowed its scope away from "the four direct-GPIO buttons," since
  "buttons need a station to hand their edges to," and deferred them to
  "whichever later sub-project builds full per-channel turnout station
  wiring."

Both deferrals point at the same piece of work. Neither a button nor an
LED composition helper is independently useful without the other — a
button needs a station to feed its edge into, and a station needs real
LED outputs to drive — so this sub-project builds both together.

## Problem

`src/esp32node4/main.cpp` currently wires only commissioning (serial +
wireless) and the composite setup trigger. It constructs no turnout
hardware at all. The node board needs 4 real per-channel turnout stations
— button in, LED pair out — using classes that already exist
(`ToggleTurnoutStation`, confirmed channel-count-agnostic and already
taking an individual button/green-output/red-output per instance — no
change needed) plus one new composition helper for the LED side
(`IndependentLedPairStation`, mirroring the panel's `LedPairStation`
exactly, since sub-project #2 built the driver/output pair but not this
GPIO-owning wrapper).

**Command port gap.** `ToggleTurnoutStation` requires a
`TurnoutCommandPort&`. The node board has no MQTT wiring yet, and
`Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource` are still
hard-bound to `NodeConfig::kChannelCount` (12) — a gap sub-project #1's
`CLAUDE.md` section already flags as its own future sub-project. Decided
with Paige: use the existing `NullTurnoutCommandPort` (no-op) for now.
This sub-project proves the button-to-LED mechanics work on real
hardware; wiring real Loco2MQTT send/receive is a separate, later
sub-project. Consequence: with no real feedback source, every station
stays on its post-`begin()` unconfirmed blink indefinitely — there is
nothing yet that would ever call `applyFeedback()`. That's expected, not
a bug, given a no-op command port.

## Decision

### `IndependentLedPairStation` (`lib/McsEsp32/src/adapters/`)

A direct mirror of `LedPairStation`
(`lib/McsEsp32/src/adapters/LedPairStation.h`/`.cpp`), owning two real
GPIOs instead of one:

```cpp
struct IndependentLedPairConfig
{
    int redGpioPin;
    int greenGpioPin;
};

class IndependentLedPairStation
{
public:
    IndependentLedPairStation(const IndependentLedPairConfig& config, Clock& clock,
                               unsigned long blinkIntervalMs, LedPairColor defaultColor);

    void begin();
    void update();
    void setIdentifying(bool active);

    DigitalOutput& green();
    DigitalOutput& red();
    IndependentLedPairDriver& driver();

private:
    ArduinoDigitalOutput redGpio_;
    ArduinoDigitalOutput greenGpio_;
    IndependentLedPairDriver driver_;
    IndependentLedPairOutput green_;
    IndependentLedPairOutput red_;
};
```

`#ifdef ARDUINO`-guarded, same as `LedPairStation`. Zero new logic — pure
construction and forwarding, identical shape to its predecessor, which
also has no native test suite for the same reason.

### `src/esp32node4/main.cpp` wiring

Follows the panel's established `TURNOUT_CONFIGS[]` + range-`for` pattern
exactly (`src/esp32/main.cpp`'s `TurnoutPanelConfig`/`stations[]` loops),
with GPIO numbers from `docs/pcb/turnout-node-4ch-pcb-design.md` §4's pin
map:

| Channel | Button GPIO | Red GPIO | Green GPIO |
|---|---|---|---|
| T1 | 34 | 32 | 33 |
| T2 | 35 | 25 | 26 |
| T3 | 36 | 27 | 13 |
| T4 | 39 | 23 | 19 |

- A `TurnoutNodeConfig[4]` table (address, name, button/red/green GPIO per
  row).
- 4× `ArduinoDigitalInput` buttons, `(pin, /*activeLow=*/true,
  /*useInternalPullup=*/false)` — matching the hardware spec's "external
  10 kΩ pull-up, no internal pull-up" decision (§2.2), the same
  constructor form the panel's matrix column inputs already use.
- 4× `IndependentLedPairStation`, reusing `BLINK_INTERVAL_MS = 500` and
  `DEFAULT_LED_COLOR = LedPairColor::Red` — the same constants, same
  values, the panel already uses.
- A single `NullTurnoutCommandPort turnoutCommandPort;`, shared across all
  4 stations (mirroring how the panel shares one real command port across
  its 12).
- 4× the **unmodified** `ToggleTurnoutStation`, constructed
  `(address, name, button, ledStation.green(), ledStation.red(),
  systemClock, turnoutCommandPort)` — green = closed, red = thrown, the
  same color convention the panel uses.

`setup()` calls `begin()` on every button, every `IndependentLedPairStation`,
and every `ToggleTurnoutStation` (in that dependency order). `loop()` calls
`update()` on every LED station and every station, placed after the
existing `WirelessSetup` branch's `return;` (added in sub-project #3) —
so they only run outside wireless-setup mode, consistent with the node
board having no LED-based boot-mode or identify feedback yet (that's
sub-project #5's job). No feedback polling, no `clearIndicator()` gating
loop — there is no MQTT/collision concept on the node board yet for
either of those to react to.

## Testing

**Zero new native tests.** `IndependentLedPairStation` is pure
construction/forwarding with no logic of its own to test, exactly
mirroring `LedPairStation`'s precedent (no native test suite exists for
it either). `main.cpp` is a composition root, verified by build only,
matching every prior `main.cpp` change in this project.

Verification is `pio run -e esp32node4` succeeding, plus confirming
`pio run -e esp32dev` and `pio run -e megaatmega2560` are unaffected, and
`pio test -e native` staying green (unaffected — nothing new is
native-testable, but it's the final sanity check).

## Non-goals

- Real Loco2MQTT send/receive (own future sub-project, per the "Command
  port gap" decision above).
- Boot-mode indicator and identify-overlay wiring for the new LEDs
  (sub-project #5).
- Status and activity LEDs (own future sub-project).
- Commissioning-form changes for 4 channels (own future sub-project).
- Any change to `ToggleTurnoutStation`, `LedPairStation`,
  `IndependentLedPairDriver`, `IndependentLedPairOutput`,
  `NullTurnoutCommandPort`, or `src/esp32/main.cpp` — all reused
  completely unmodified.
