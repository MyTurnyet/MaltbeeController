# 4-Turnout Node: Composite Setup Trigger (Sub-project #3) — Design

This is sub-project **#3** of the 4-turnout node board effort (see
`docs/superpowers/specs/2026-10-21-esp32-node4-channel-count-design.md` for
sub-project #1 and
`docs/superpowers/specs/2026-10-21-esp32-node4-independent-led-pair-driver-design.md`
for sub-project #2). It depends on neither directly — it's new, independent
adapter logic plus a `main.cpp` wiring change — and resolves a known gap
those two left behind.

## Scope narrowed from the original roadmap

The roadmap item this sub-project was planned against bundled two things:
"Four direct inputs through the existing `DigitalInput` port, wired in
place of the matrix, and a small composite `DigitalInput`... fed to the
unchanged `ButtonSetupModeTrigger`." Discussed with Paige: these two halves
aren't equally buildable right now. `ButtonSetupModeTrigger` only depends
on a `DigitalInput&` and a `Clock&` — nothing about turnout stations — so
the composite-input-plus-trigger half is fully self-contained today. The
four direct-GPIO buttons have nothing to wire *to* yet: there is no
per-channel turnout-station object on the node board (sub-project #2
deliberately deferred that composition helper), so declaring four
`ArduinoDigitalInput`s now would be dead code. **Decision: split the
roadmap item.** This sub-project is the composite input and the trigger
wiring only. The four direct-GPIO buttons move to whichever later
sub-project builds full per-channel turnout station wiring on the node
board (buttons need a station to hand their edges to).

## Problem

`src/esp32node4/main.cpp` (sub-project #1's placeholder) constructs
`CaptivePortalServer4`/`WebFormCommissioningAdapter4` and has working
`BootMode::WirelessSetup` branches in `setup()`/`loop()`, but nothing ever
calls `NvsSetupModeRequestStore::requestOnNextBoot()` to actually select
that boot mode — `CLAUDE.md`'s sub-project #1 section already documents
this as a known, accepted gap: wireless commissioning is wired but
dormant. The 12-channel panel reaches `WirelessSetup` via a
`ButtonSetupModeTrigger` watching its BOOT button (GPIO0) for a 3-second
hold-then-release. `docs/pcb/turnout-node-4ch-pcb-design.md` §2.7/§3.4
decided the node board gets a *second* way in: a dedicated SETUP button on
GPIO16 (internal pull-up, no external parts), specifically because GPIO0
is a strapping pin not exposed on the 30-pin kit's header except as the
kit's own BOOT button — a labelled, panel-mountable button needs its own
GPIO. Both buttons must work, so `ButtonSetupModeTrigger` (which takes a
single `DigitalInput&`) needs one input that's active when *either* real
button is held.

## Decision

A small new adapter, `EitherActiveDigitalInput`, composes two
`DigitalInput&`s and is active when either is. `ButtonSetupModeTrigger`
itself is **not modified** — it already takes any `DigitalInput&`, so
handing it this composite is the entire integration; its hold/release-
settle logic is reused completely unchanged, exactly as the design
originally called for.

Lives in `lib/McsEsp32/src/adapters/`, matching where `GatedDigitalInput`
already lives — logic that's abstractly portable (no Arduino dependency)
but motivated entirely by this ESP32 board family, following this
project's existing precedent of keeping such classes in `McsEsp32` rather
than promoting them to `McsCore`.

**Exactly two inputs, not a variadic list.** The actual need — "either the
BOOT or GPIO16 input" — is for precisely two. A general N-input composite
would be unused generality; if a third setup-trigger input is ever needed,
extending to N is a small, separate change made when there's an actual
third input to wire.

### `main.cpp` wiring

`src/esp32node4/main.cpp` currently has no `Clock` at all — nothing
constructed so far needed one. This sub-project adds:

- `ArduinoClock systemClock;` (same type the panel uses).
- `ArduinoDigitalInput bootButton(0, /*activeLow=*/true,
  /*useInternalPullup=*/true);` — same construction as the panel's.
- `ArduinoDigitalInput setupButton(16, /*activeLow=*/true,
  /*useInternalPullup=*/true);` — the new dedicated SETUP button, per
  §3.4's pin/pull-up decision.
- `EitherActiveDigitalInput setupTriggerInput(bootButton, setupButton);`
- `ButtonSetupModeTrigger setupTrigger(setupTriggerInput, systemClock,
  SETUP_TRIGGER_HOLD_MS);` with `SETUP_TRIGGER_HOLD_MS = 3000`, matching
  the panel's constant.

`setup()` calls `bootButton.begin()` and `setupButton.begin()`
unconditionally, before the `BootMode::WirelessSetup` check — mirroring
the panel's pattern of doing GPIO hardware init regardless of boot mode,
only gating higher-level behavior on it.

`loop()` gains the panel's exact trigger-check block, verbatim:

```cpp
setupTrigger.update();

if (setupTrigger.requested())
{
    const bool stored = setupModeRequestStore.requestOnNextBoot();
    if (stored)
    {
        uartPort.write("Entering wireless setup...\n");
        Serial.flush();
        ESP.restart();
    }
    else
    {
        uartPort.write("Failed to persist wireless setup request; staying in normal mode.\n");
    }
}
```

The existing `BootMode::WirelessSetup` branch in `loop()` gains a `return;`
after `captivePortalServer.poll();`, so this trigger-check block only runs
outside wireless-setup mode — matching the panel, where the analogous
branch already returns early for the same reason (don't re-detect the
gesture while already in the mode it requests). Today this `return` is a
behavior-preserving addition (nothing currently follows that `if` block),
but it's required once the trigger-check block is appended after it.

## Components

### `EitherActiveDigitalInput` (`lib/McsEsp32/src/adapters/EitherActiveDigitalInput.h` / `.cpp`)

```cpp
class EitherActiveDigitalInput final : public DigitalInput
{
public:
    EitherActiveDigitalInput(DigitalInput& first, DigitalInput& second);

    [[nodiscard]] bool isActive() const override;

private:
    DigitalInput& first_;
    DigitalInput& second_;
};
```

## Testing

Fully native-testable with the existing `FakeDigitalInput` (`test/support/`)
— no new fakes needed.

`EitherActiveDigitalInput`:
- Both inputs inactive → inactive.
- First active, second inactive → active.
- First inactive, second active → active.
- Both active → active.

`main.cpp` has no native test target (composition root, same as every
prior `main.cpp` change in this project — see `CLAUDE.md`'s precedent for
`src/esp32/main.cpp` changes). Verified instead by `pio run -e esp32node4`
succeeding, plus `pio run -e esp32dev` and `pio run -e megaatmega2560`
staying unaffected.

## Non-goals

- The four direct-GPIO turnout buttons — moved to whichever later
  sub-project builds full per-channel turnout station wiring on the node
  board (see "Scope narrowed" above).
- Any turnout station, LED, or status/activity LED wiring.
- Any change to `ButtonSetupModeTrigger`, `ArduinoDigitalInput`,
  `ArduinoClock`, `NvsSetupModeRequestStore`, or the 12-channel panel's
  `src/esp32/main.cpp` — all reused completely unmodified.
