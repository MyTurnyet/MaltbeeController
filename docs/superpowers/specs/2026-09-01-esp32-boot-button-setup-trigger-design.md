# ESP32 BOOT-Button Wireless Setup Trigger (Sub-project #2c-c) — Design

This replaces the T1+T2 combo-button wireless-setup trigger (sub-project
#2c, `ComboSetupModeTrigger`) with a dedicated single-button gesture on the
ESP32 board's own BOOT button (GPIO0), and adds LED feedback while wireless
setup mode is active — something the current implementation has never had.
Modeled directly on the equivalent, already-shipped feature in the sibling
project, `MaltbeeTurnoutController` (`lib/McsCore/src/adapters/ButtonSetupModeTrigger.h`).

## Context

`docs/superpowers/specs/2026-08-29-esp32-wireless-setup-trigger-design.md`
chose the T1+T2 combo specifically because this panel has no spare GPIO —
all 19 usable pins were already allocated to the 3×4 matrix and 12 LED
pairs. That design accepted a documented gap: a human's staggered
two-finger press can fire one ordinary toggle command on whichever button
was pressed first, before the combo's suppression engages.

Comparing against `MaltbeeTurnoutController`, which triggers the same
gesture off the ESP32 dev board's own physical BOOT button (GPIO0) instead,
made clear this panel doesn't actually need a spare GPIO for this — the
BOOT button is already present on the board (an ELEGOO ESP32-WROOM-32 dev
board, same as this project targets) at zero wiring cost, and this
project's own GPIO table already lists GPIO0 as "intentionally avoided"
only because it's a *boot-strapping* pin, not because it's inaccessible at
runtime. Reading it live during `loop()`, well after boot has completed
normally, is exactly what `MaltbeeTurnoutController` already does safely
(see that class's own header comment for why a boot-time strapping read
can't work: holding GPIO0 low through power-on/reset puts the ROM into
permanent UART download mode instead of running application code — so this
must be a live gesture, not something checked once at startup).

Switching to a dedicated button also removes the entire reason
`ComboSetupModeTrigger`'s staggered-press gap existed: two independent
inputs, held together, needing joint hold-timing logic. A single button
has no such case.

Separately, the ESP32 panel has never had any LED indication that it's
currently in wireless setup mode — `loop()` simply services the captive
portal and returns. `MaltbeeTurnoutController` blinks a dedicated status
LED at a fast, distinct rate for exactly this reason. This panel has no
spare status LED (all 12 GPIOs are already the turnout indicator pairs),
but it already has a fast, synchronized, all-pairs flash pattern built for
an unrelated feature — `LedPairDriver::setIdentifying()` (sub-project
#2d-b, identify-blink) — and reusing it costs no new domain code.

## Decisions (confirmed via Q&A)

1. **Trigger: the ESP32's onboard BOOT button (GPIO0), held ≥3 seconds,
   fires on release** — replacing the T1+T2 combo. No new wiring; the
   board's own BOOT button is used as-is, `INPUT_PULLUP`, active-low.
2. **LED feedback: reuse `LedPairDriver::setIdentifying()`,** the same
   fast (150ms) all-12-pairs green/red flash already used for MQTT
   identify-blink, rather than inventing a third, visually distinct
   pattern. Wireless setup mode and MQTT identify-blink can never overlap
   (setup mode never starts MQTT), so sharing the exact visual is
   unambiguous in practice and needs zero new `LedPairDriver` code.
3. **`ComboSetupModeTrigger` is deleted, not deprecated-in-place** — once
   the swap lands, nothing references it, and this project's convention is
   to delete confirmed-dead code rather than leave it orphaned.
4. **All 12 turnout buttons become uniformly suppressed by collision
   alone.** The BOOT button is physically independent of the 12-button
   matrix, so there is no longer any reason to special-case T1/T2's
   suppression during a setup-trigger hold — the per-tick suppression loop
   becomes one uniform line instead of a T1/T2 special case plus a
   10-button general case.

## Components

### `ButtonSetupModeTrigger` (`lib/McsEsp32/src/adapters/ButtonSetupModeTrigger.h`/`.cpp`, new)

Same name as `MaltbeeTurnoutController`'s equivalent class for easy
cross-project recognition, but built against *this* codebase's existing
idioms — `DigitalInput::isActive()` and `Clock::nowMilliseconds()` — rather
than porting the sibling's `Level`/`Instant`/`Duration` domain types
verbatim. It's the same press/hold/release state machine
`ComboSetupModeTrigger` already has, minus the second button and the
joint-hold-anchoring logic that only matters when two independent inputs
must both be active at once:

```cpp
#pragma once

#include "ports/Clock.h"
#include "ports/DigitalInput.h"

class ButtonSetupModeTrigger
{
public:
    ButtonSetupModeTrigger(DigitalInput& button, Clock& clock, unsigned long minHoldMs);

    void update();
    [[nodiscard]] bool requested() const;

private:
    DigitalInput& button_;
    Clock& clock_;
    unsigned long minHoldMs_;
    bool holding_ = false;
    unsigned long holdStartMs_ = 0;
    bool requestedThisTick_ = false;
};
```

```cpp
#include "ButtonSetupModeTrigger.h"

ButtonSetupModeTrigger::ButtonSetupModeTrigger(DigitalInput& button, Clock& clock,
                                                const unsigned long minHoldMs)
    : button_(button), clock_(clock), minHoldMs_(minHoldMs)
{
}

void ButtonSetupModeTrigger::update()
{
    requestedThisTick_ = false;
    const bool active = button_.isActive();

    if (active && !holding_)
    {
        holding_ = true;
        holdStartMs_ = clock_.nowMilliseconds();
    }
    else if (!active && holding_)
    {
        holding_ = false;
        const unsigned long heldFor = clock_.nowMilliseconds() - holdStartMs_;
        if (heldFor >= minHoldMs_)
        {
            requestedThisTick_ = true;
        }
    }
}

bool ButtonSetupModeTrigger::requested() const
{
    return requestedThisTick_;
}
```

No `isHolding()` accessor — nothing needs it, since (per Decision 4) no
button suppression depends on the setup gesture being mid-hold anymore.

### `src/esp32/main.cpp` changes

New global (placed where `matrixCol0`..`matrixCol3` are declared, since
it's the same kind of raw GPIO input):

```cpp
ArduinoDigitalInput bootButton(0, /*activeLow=*/true, /*useInternalPullup=*/true);
```

`bootButton.begin();` added alongside the existing matrix
row/column `.begin()` calls in `setup()`.

Trigger construction changes from:

```cpp
ComboSetupModeTrigger setupTrigger(matrixButtons[0], matrixButtons[1], systemClock, SETUP_TRIGGER_HOLD_MS);
```

to:

```cpp
ButtonSetupModeTrigger setupTrigger(bootButton, systemClock, SETUP_TRIGGER_HOLD_MS);
```

`SETUP_TRIGGER_HOLD_MS` (3000) is unchanged. The `setupTrigger.update()`
call site and the `if (setupTrigger.requested())` persist-and-reboot block
are unchanged — only the trigger's construction and inputs differ.

The per-tick suppression loop simplifies from:

```cpp
gatedButtons[0].setSuppressed(setupTrigger.isHolding() || collision);
gatedButtons[1].setSuppressed(setupTrigger.isHolding() || collision);
for (int i = 2; i < 12; ++i)
{
    gatedButtons[i].setSuppressed(collision);
}
```

to:

```cpp
for (auto& gated : gatedButtons)
{
    gated.setSuppressed(collision);
}
```

In the `BootMode::WirelessSetup` branch of `loop()`, add the identify-flash
calls before the existing early `return`:

```cpp
if (bootMode == BootMode::WirelessSetup)
{
    captivePortalServer.poll();
    for (auto& ledStation : ledStations)
    {
        ledStation.setIdentifying(true);
        ledStation.update();
    }
    return;
}
```

`setIdentifying(true)` is already idempotent (guards on `active ==
identifying_`, per #2d-b), so calling it unconditionally every tick is
safe and matches how the normal-mode identify loop already calls it.
`update()` must still be called per station each tick to actually advance
the flash timing and write the GPIO — the normal-mode path does this via
its own separate `ledStation.update()` loop, which the `WirelessSetup`
branch never reaches (it returns before that point), so it needs its own
call here.

### Removed

- `lib/McsEsp32/src/adapters/ComboSetupModeTrigger.h`/`.cpp`
- `test/test_combo_setup_mode_trigger/`

## Testing

- **`ButtonSetupModeTrigger`** (new suite, mirrors
  `test_combo_setup_mode_trigger` minus the two-button-specific cases):
  not holding initially; holding but released before `minHoldMs` does not
  request; held for exactly `minHoldMs` then released requests once, then
  resets on the next `update()`; a fresh press-hold-release cycle after a
  full release can request again; the hold survives intermediate `update()`
  ticks without restarting the timer.
- **`src/esp32/main.cpp`**: no native test (matches existing convention) —
  verified via `pio run -e esp32dev` build success and a manual
  read-through, same as every other composition-root change to this file.

## File layout

- `lib/McsEsp32/src/adapters/ButtonSetupModeTrigger.h`/`.cpp` (new)
- `test/test_button_setup_mode_trigger/` (new)
- Delete: `lib/McsEsp32/src/adapters/ComboSetupModeTrigger.h`/`.cpp`,
  `test/test_combo_setup_mode_trigger/`
- Modify: `src/esp32/main.cpp` (new `bootButton` global + `begin()` call,
  swapped trigger construction, simplified suppression loop, LED flash
  calls in the `WirelessSetup` branch)
- Modify: `CLAUDE.md` (ESP32 composition-root and adapters descriptions —
  drop `ComboSetupModeTrigger`, describe `ButtonSetupModeTrigger` and the
  wireless-setup LED flash; remove the now-resolved staggered-press
  "Known gap" reference)
- Modify: `docs/HARDWARE_BRINGUP_CHECKLIST.md` section 2.4 — BOOT button
  gesture instead of T1+T2, and the expected LED flash
- Modify: `docs/superpowers/specs/2026-08-29-esp32-wireless-setup-trigger-design.md`
  — add a one-line pointer at the top noting it's superseded by this spec
  (kept as history, not deleted)

## Non-goals

- Any change to `LedPairDriver`, `LedPairStation`, or `IdentifyModeTimer` —
  the existing `setIdentifying()` override is reused exactly as built for
  #2d-b, with no new parameters or modes.
- A distinct visual pattern for wireless setup vs. MQTT identify-blink
  (Decision 2) — the two states cannot co-occur, so sharing one pattern is
  accepted rather than engineered around.
- Any change to `NvsSetupModeRequestStore`, `BootModeSelector`, or the
  persist-and-reboot flow itself — all reused exactly as built for #2c.
- Adding an `isHolding()`-style accessor to `ButtonSetupModeTrigger` — no
  caller needs mid-hold state once button suppression no longer depends on
  it (Decision 4).
