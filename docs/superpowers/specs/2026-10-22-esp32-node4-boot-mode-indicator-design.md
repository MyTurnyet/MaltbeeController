# 4-Turnout Node: Boot-Mode LED Indicator (Sub-project #5) — Design

This is sub-project **#5** of the 4-turnout node board effort. It depends
on sub-project #2
(`docs/superpowers/specs/2026-10-21-esp32-node4-independent-led-pair-driver-design.md`,
`IndependentLedPairDriver`) and sub-project #4
(`docs/superpowers/specs/2026-10-22-esp32-node4-turnout-station-wiring-design.md`,
the real `ledStations[4]` it wires into `main.cpp`).

## Scope narrowed from the original roadmap

The roadmap item this sub-project was planned against was "Boot-mode,
setup and identify feedback for the new LEDs" — two mechanisms that are
not equally buildable right now. Discussed with Paige: on the 12-channel
panel, `BootModeIndicator`/`LedPairFlashIndicator<N>` is driven purely by
the locally-known `BootMode` value, computed at boot with no MQTT
dependency at all. The identify-overlay (`IdentifyModeTimer` plus a
`setIdentifying()` loop in `loop()`) is triggered by an MQTT subscription
callback on `panel/<nodeId>/identify` — the node board has no WiFi/MQTT
wiring at all yet (`Loco2MqttTurnoutCommandAdapter`/`Loco2MqttFeedbackSource`
still need the `Basic<X><N>` treatment, a gap `CLAUDE.md` already flags as
its own future sub-project). **Decision: this sub-project is the
boot-mode indicator only.** Identify-overlay wiring moves to whichever
future sub-project adds real Loco2MQTT wiring to the node board.

## Problem

`src/esp32node4/main.cpp` computes `bootMode` at boot (sub-project #1) but
nothing gives it a visual on the 4 independent-output LED pairs sub-project
#4 just wired in. The panel already solves this for shared-GPIO pairs via
`BootModeIndicator` (port) / `LedPairFlashIndicator<N>` (template
implementation) — `show(mode)` is called once when boot mode is decided,
`update()` every tick. The node board needs the equivalent for
`IndependentLedPairDriver`.

## Decision

A near-exact mirror of `LedPairFlashIndicator<N>`
(`lib/McsEsp32/src/adapters/LedPairFlashIndicator.h`), templated against
`IndependentLedPairDriver*` instead of `LedPairDriver*`. The semantics
transfer with **no new design question**, because of a decision already
made in sub-project #2: `show(BootMode::NeedsCommissioning)` calls
`setGreen(false)`/`setRed(false)` on every pair — against an
`IndependentLedPairDriver` already in its resting `Mode::Blink` state
(the state every station's `begin()`/`clearIndicator()` already leaves it
in), that's a no-op, so `NeedsCommissioning` shows the exact same
red/green alternating blink the unconfirmed state already shows. That's
consistent, not a regression — it's the same "keep the existing blink
visual" choice sub-project #2 already made, just inherited here rather
than re-decided.
`show(BootMode::WirelessSetup)` calls `setIdentifying(true)` on every
pair, giving the same fast 150 ms flash the panel's identify-blink
override already provides, exactly mirroring the panel's own
`WirelessSetup`-mode LED feedback (sub-project #2c-c). `update()` is a
no-op body, identical to its predecessor, since `IndependentLedPairStation::update()`
already drives the flash timing every tick.

**Correction (discovered during implementation):** this assumption was
wrong for the `BootMode::WirelessSetup` path specifically —
`src/esp32node4/main.cpp`'s `loop()` returned early in that branch before
ever reaching the `ledStations` update loop, so the flash never actually
ticked during wireless setup until the `WirelessSetup` branch itself
gained an explicit `ledStation.update()` loop (mirroring
`src/esp32/main.cpp`'s identical branch). See the "Boot-mode LED
indicator (sub-project #5)" section of CLAUDE.md for the final, correct
behavior.

Lives in `lib/McsEsp32/src/adapters/`, matching where
`LedPairFlashIndicator` already lives.

### `main.cpp` wiring

Mirrors the panel's exact pattern:

```cpp
const std::array<IndependentLedPairDriver*, 4> bootModeLedPairs = {
    &ledStations[0].driver(), &ledStations[1].driver(),
    &ledStations[2].driver(), &ledStations[3].driver(),
};
IndependentLedPairFlashIndicator<4> bootModeIndicator(bootModeLedPairs);
```

`setup()`: `bootModeIndicator.show(bootMode);` called right after the
`ledStation`/`station` `begin()` loops, before the `BootMode::WirelessSetup`
check — matching where the panel calls it.

`loop()`: `bootModeIndicator.update();` added as the very first line,
before `serialCommissioningAdapter.poll()` — matching the panel exactly,
even though the body is a no-op today. This is deliberate consistency
with the established pattern (the panel's own comment notes `update()`
exists for future flash-timing needs the current implementation doesn't
use), not a functional requirement — but keeping the call site identical
means no divergence to reconcile if `IndependentLedPairFlashIndicator`
ever grows real per-tick logic.

## Components

### `IndependentLedPairFlashIndicator<N>` (`lib/McsEsp32/src/adapters/IndependentLedPairFlashIndicator.h`)

```cpp
template <std::size_t N>
class IndependentLedPairFlashIndicator : public BootModeIndicator
{
public:
    explicit IndependentLedPairFlashIndicator(const std::array<IndependentLedPairDriver*, N>& pairs);

    void show(BootMode mode) override;
    void update() override;

private:
    void applyIdentifying(bool identifying);
    void releaseAllPairs();

    std::array<IndependentLedPairDriver*, N> pairs_;
};
```

Header-only template, same as `LedPairFlashIndicator`.

## Testing

Fully native-testable with the existing `FakeClock`/`FakeDigitalOutput`
(`test/support/`) — no new fakes needed, and no mocking of
`IndependentLedPairDriver` itself: tests construct real driver instances
over fake GPIOs, exactly mirroring `test_led_pair_flash_indicator`'s own
approach.

New suite, `test_independent_led_pair_flash_indicator`, mirroring
`test_led_pair_flash_indicator`'s 3 cases:
- `show(WirelessSetup)` starts the identify flash on every pair (asserts
  green GPIO set, red GPIO clear, for each pair).
- `show(NeedsCommissioning)` starts the unconfirmed blink on every pair
  (steady-green pairs transition to blink; after one interval elapses and
  `update()` runs, asserts each pair now shows red, not green).
- `show(Normal)` leaves identifying off, so a later direct `setGreen()`
  call still takes effect (proving `show(Normal)` didn't leave the
  indicator in a state that suppresses ordinary station-driven writes).

`main.cpp` wiring itself has no native test — composition root, verified
by `pio run -e esp32node4` build, matching every prior `main.cpp` change
in this project.

## Non-goals

- Identify-overlay wiring (`IdentifyModeTimer`, the MQTT
  `panel/<nodeId>/identify` subscription, a `setIdentifying()` loop driven
  by it) — deferred to whichever future sub-project adds real Loco2MQTT
  wiring to the node board.
- Real WiFi/MQTT of any kind.
- Any change to `LedPairFlashIndicator`, `BootModeIndicator`,
  `IndependentLedPairDriver`, `IndependentLedPairStation`, or
  `src/esp32/main.cpp` — all reused completely unmodified.
- Status and activity LEDs, commissioning-form changes, the bring-up
  checklist — separate, later sub-projects per the original roadmap.
