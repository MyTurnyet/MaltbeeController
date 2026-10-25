# ESP32 Node4 Status/Activity LEDs — Design

**Status:** Approved
**Sub-project:** #10 in the 4-turnout node board effort's own numbering (chronologically after #1–#9; see `CLAUDE.md`'s "4-channel node board: channel-count templating (sub-project #1)" section for the numbering convention — distinct from the unrelated, older panel-focused "#9a"/"#9b"/"#9c" sequence that happens to reuse nearby digits)

## Problem

`docs/pcb/turnout-node-4ch-pcb-design.md` §7 lists item 5, "Status and
activity LEDs," as the one remaining open firmware item on the node4
board — hardware already decided (§3.3): a network status LED on GPIO 18
and an activity LED on GPIO 17, both 5mm/220Ω, upright on the service edge.
A separate power LED (1kΩ, ~3mA) sits across the 5V rail independent of
software and is not a firmware concern.

Unlike every prior node4 sub-project since #9, **this one has no panel
equivalent to mirror.** The panel has no on-board status/activity LEDs at
all — only its 12 turnout LED pairs — which is exactly the distinction the
PCB doc draws: "this is also where wireless-setup and WiFi/broker state get
their own indicator **instead of borrowing the turnout LEDs**." This
sub-project is new design, not a mirroring exercise.

§4.2's peripheral-conflict table already flags both GPIOs (VSPI defaults to
18; UART2 TX defaults to 17) as needing explicit-pin care if any future
`Wire.begin()`/`SPI.begin()`/`Serial2.begin()` call is added — a pre-existing
caution this sub-project does not need to resolve, since no such call exists
in `src/esp32node4/main.cpp` today.

## Decision

### Behavior

**Network status LED (GPIO 18)** reflects connection stage, with collision
detection as the highest-priority override:

| Condition | State |
|---|---|
| `nodeId` collision latched | `FastBlink` (150ms) |
| WiFi not connected | `Off` |
| WiFi connected, MQTT broker not connected | `SlowBlink` (500ms, matching `BLINK_INTERVAL_MS`'s existing rate) |
| WiFi and MQTT both connected | `On` (steady) |

**Activity LED (GPIO 17)** gives a momentary (150ms) flash on every
Loco2MQTT command sent *or* feedback received — a visible "the board is
genuinely talking to the broker" signal, distinct from the network LED's
connectivity-only view. A flash arriving before a prior one expires extends
the window rather than stacking (inherited for free from `IdentifyModeTimer`'s
own existing, already-tested behavior).

**`BootMode::WirelessSetup`:** both LEDs simply stay off. WiFi/MQTT are
never started in that mode, so the network LED's state computation reads
`Off` naturally (no WiFi connection ever attempted) and no traffic ever
exists to trigger the activity flash — no new branch is needed. The
existing 4-turnout-LED flash (sub-project #5) remains the sole setup-mode
indicator, per the scope decision made with Paige during brainstorming.

### New library code

**`lib/McsEsp32/src/domain/StatusLedDriver.h`/`.cpp`** — a new, generic,
native-testable domain class. Drives one single-color LED (one real GPIO)
through four states:

```cpp
enum class State
{
    Off,
    On,
    SlowBlink,
    FastBlink
};

class StatusLedDriver
{
public:
    StatusLedDriver(DigitalOutput& gpio, Clock& clock, unsigned long slowBlinkIntervalMs,
                     unsigned long fastBlinkIntervalMs);

    void begin();
    void setState(State state);
    void update();

private:
    void applyGpio();

    DigitalOutput& gpio_;
    Clock& clock_;
    unsigned long slowBlinkIntervalMs_;
    unsigned long fastBlinkIntervalMs_;

    State state_ = State::Off;
    bool blinkShowingOn_ = true;
    unsigned long lastToggleTime_ = 0;
};
```

Mirrors `LedPairDriver`'s existing toggle-timing shape (`clock_.nowMilliseconds()`
comparisons, re-stamping `lastToggleTime_` on a state change so a transition
takes visible effect immediately rather than waiting out a stale toggle
boundary) but is deliberately simpler: one color, one GPIO, and a genuine
`Off` state — `LedPairDriver`'s "exactly one color is always lit" invariant
does not apply here and would be the wrong shape to reuse or extend.
`update()` is a no-op when `state_` is `Off` or `On` (matching
`LedPairDriver::update()`'s own early-return-when-not-blinking shape);
`setState()` is a no-op when called with the state already in effect,
preventing a stale `update()` call from fighting a fresh `setState()` call
within the same tick — mirroring `IndependentLedPairDriver::setIdentifying()`'s
existing idempotent-call pattern.

**`lib/McsEsp32/src/adapters/ActivityTrackingTurnoutCommandPort.h`** — a new,
header-only decorator (no `.cpp`, same shape as `GatedDigitalInput`'s
trivial-forwarding style):

```cpp
class ActivityTrackingTurnoutCommandPort : public TurnoutCommandPort
{
public:
    ActivityTrackingTurnoutCommandPort(TurnoutCommandPort& inner, IdentifyModeTimer& activityTimer)
        : inner_(inner), activityTimer_(activityTimer)
    {
    }

    void send(int address, TurnoutPosition position) override
    {
        inner_.send(address, position);
        activityTimer_.trigger();
    }

private:
    TurnoutCommandPort& inner_;
    IdentifyModeTimer& activityTimer_;
};
```

Wraps `turnoutCommandPort4` before it is handed to the 4 `ToggleTurnoutStation`s
— no change to `ToggleTurnoutStation`, `ToggleTurnoutControl`, or
`Loco2MqttTurnoutCommandAdapter4`. This is the clean hexagonal-architecture
way to observe outbound traffic at the composition-root boundary without
touching any already-tested production class that both the panel and the
node board depend on.

**`IdentifyModeTimer` is reused as-is, unmodified**, for the activity LED's
momentary flash — it already provides exactly "trigger() stamps the clock,
isActive() compares against a fixed duration, a second trigger() before
expiry extends rather than stacks," with no identify-specific logic baked
into its implementation (confirmed by reading its header/`.cpp` before this
design was written). No new timer class is needed for this purpose.

### `src/esp32node4/main.cpp` wiring

New constants (anonymous namespace, alongside the existing ones):

```cpp
constexpr int NETWORK_STATUS_LED_GPIO = 18;
constexpr int ACTIVITY_LED_GPIO = 17;
constexpr unsigned long NETWORK_LED_SLOW_BLINK_MS = 500;
constexpr unsigned long NETWORK_LED_FAST_BLINK_MS = 150;
constexpr unsigned long ACTIVITY_FLASH_DURATION_MS = 150;
```

New globals, declared alongside `bootModeIndicator` (before `wifiLink`):

```cpp
ArduinoDigitalOutput networkStatusLedGpio(NETWORK_STATUS_LED_GPIO, /*activeLow=*/false);
ArduinoDigitalOutput activityLedGpio(ACTIVITY_LED_GPIO, /*activeLow=*/false);
StatusLedDriver networkStatusLedDriver(networkStatusLedGpio, systemClock, NETWORK_LED_SLOW_BLINK_MS,
                                         NETWORK_LED_FAST_BLINK_MS);
IdentifyModeTimer activityFlashTimer(systemClock, ACTIVITY_FLASH_DURATION_MS);
```

(`activeLow=false` matches the existing LED-GPIO convention already used by
`IndependentLedPairStation.cpp`'s own `redGpio_(config.redGpioPin, false)`/
`greenGpio_(config.greenGpioPin, false)`.)

A new global declared right after `feedbackSource`, wrapping `turnoutCommandPort`:

```cpp
ActivityTrackingTurnoutCommandPort activityTrackingCommandPort(turnoutCommandPort, activityFlashTimer);
```

The 4 `ToggleTurnoutStation` constructions change their last argument from
`turnoutCommandPort` to `activityTrackingCommandPort`.

`setup()`: after the existing `for (auto& station : stations) { station.begin(); }`
loop and before `bootModeIndicator.show(bootMode);`, add:

```cpp
networkStatusLedGpio.begin();
activityLedGpio.begin();
networkStatusLedDriver.begin();
```

(`activityFlashTimer` needs no `begin()` — `IdentifyModeTimer` has none.)

`loop()`: after the existing `if (configValid) { wifiLink.poll(); ... }`
block and before `presenceAnnouncer.update(...)`, add:

```cpp
StatusLedDriver::State networkLedState;
if (collision)
{
    networkLedState = StatusLedDriver::State::FastBlink;
}
else if (!wifiLink.connected())
{
    networkLedState = StatusLedDriver::State::Off;
}
else if (!mqttLink.connected())
{
    networkLedState = StatusLedDriver::State::SlowBlink;
}
else
{
    networkLedState = StatusLedDriver::State::On;
}
networkStatusLedDriver.setState(networkLedState);
networkStatusLedDriver.update();
```

Inside the existing `while (feedbackSource.poll(feedback)) { ... }` loop
body, add one line triggering the activity flash on every received feedback
message (once per `poll()` iteration, not once per station):

```cpp
activityFlashTimer.trigger();
```

As the **very last statement in `loop()`**, after the existing
`for (auto& station : stations) { station.update(); }` loop (where outbound
sends actually happen via `activityTrackingCommandPort`), add:

```cpp
// Must run after the station.update() loop above, where a same-tick send
// actually reaches activityTrackingCommandPort and calls trigger(). Reading
// isActive() any earlier in this tick would show last tick's state and
// delay a fresh flash by one tick — the same ordering hazard sub-project
// #9's identify wiring documents for ledStation.update().
activityLedGpio.set(activityFlashTimer.isActive());
```

### Testing

- `test/test_status_led_driver/test_main.cpp` — new suite, native, no
  Arduino dependency. Cases: begins `Off`; `On` drives the GPIO steady high;
  `Off` drives it steady low; `SlowBlink` toggles at `slowBlinkIntervalMs`;
  `FastBlink` toggles at `fastBlinkIntervalMs` (distinct from the slow rate);
  changing state resets the blink phase and re-stamps the toggle timer
  immediately rather than waiting out a stale boundary; `update()` is a
  no-op while `Off`/`On`; redundant `setState()` calls with the same state
  don't disturb an in-progress blink's phase.
- `test/test_activity_tracking_turnout_command_port/test_main.cpp` — new
  suite, native. Cases: `send()` forwards address/position unchanged to the
  inner port (via `FakeTurnoutCommandPort`); `send()` triggers the activity
  timer; a second `send()` before the timer expires extends rather than
  stacks (relying on `IdentifyModeTimer`'s own already-tested behavior, not
  re-verifying its internals here — this test only confirms `trigger()` is
  called).
- No changes to any existing test suite.
- `pio run -e esp32node4`/`esp32dev`/`megaatmega2560` must all still build
  clean; `esp32dev`/`megaatmega2560` are expected to be byte-for-byte
  unaffected (no shared code touched outside `lib/McsEsp32`'s own new
  files, which neither of those environments' composition roots reference).
- `pio test -e native` must show 2 new suites passing alongside the
  existing 54 (56 total), with zero regressions.

### Documentation

- `CLAUDE.md` gains a new "### Status/activity LEDs (sub-project #10)"
  section, following the established pattern.
- Both now-standard staleness sweeps run as required plan steps:
  - `grep -n "future sub-project\|blocked on\|unwired\|not done\|still\|yet" CLAUDE.md docs/pcb/*.md README.md`
    (widened to include `README.md`, per the lesson sub-project #9's final
    review surfaced — the sweep's file scope, not just its keyword list,
    had a gap)
  - `grep -n "Status as of sub-project" docs/pcb/*.md`
- `docs/pcb/turnout-node-4ch-pcb-design.md` §7's status line and both
  reference lists get bumped to include this sub-project — and, since this
  sub-project closes the *last* open §7 item, the status line's framing
  changes from "item N remains open" to all items done.
- `README.md` gets reviewed once more for any node4-board claim this
  sub-project makes stale (the whole-file read sub-project #9's fix applied,
  not just a grep).

## Non-goals

- No new behavior on the panel (`src/esp32/main.cpp`) or the Mega
  (`src/mega/main.cpp`) — this hardware is node4-specific per the PCB doc.
- No change to `LedPairDriver`, `IndependentLedPairDriver`,
  `IdentifyModeTimer`, `ToggleTurnoutStation`, `ToggleTurnoutControl`, or
  `Loco2MqttTurnoutCommandAdapter4` — all reused unmodified.
- No distinct LED pattern for `BootMode::WirelessSetup` on these two new
  LEDs — they simply stay off, per the scope decision made during
  brainstorming; the existing turnout-LED flash (sub-project #5) remains
  the sole setup-mode indicator.
- No new MQTT topic, no new commissioning field.
