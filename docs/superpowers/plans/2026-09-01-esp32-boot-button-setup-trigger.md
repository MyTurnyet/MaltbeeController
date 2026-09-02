# ESP32 BOOT-Button Wireless Setup Trigger Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the ESP32 turnout panel's T1+T2 combo-button wireless-setup gesture with a single-button hold on the board's own BOOT button (GPIO0), and add LED feedback (reusing the existing identify-blink flash) while wireless setup mode is active.

**Architecture:** A new `ButtonSetupModeTrigger` adapter class (single-button hold/release timing, native-testable against `DigitalInput`/`Clock` ports) replaces `ComboSetupModeTrigger`. `src/esp32/main.cpp` gains one new raw `ArduinoDigitalInput` on GPIO0 and drives all 12 `LedPairStation`s' existing `setIdentifying()` override while the wireless-setup captive portal is running. No changes to `BootMode`, `BootModeSelector`, `NvsSetupModeRequestStore`, `CaptivePortalServer`, or `LedPairDriver`/`LedPairStation` internals — every piece this plan touches is reused or replaced as-is.

**Tech Stack:** C++17, PlatformIO (`native` for the new trigger's tests, `esp32dev` build-check for `main.cpp`), Catch2.

## Global Constraints

- `ButtonSetupModeTrigger` must be fully native-testable using existing test doubles (`FakeDigitalInput`, `FakeClock` in `test/support/`) — no new test double needed.
- `ButtonSetupModeTrigger` follows this codebase's existing idiom (`DigitalInput::isActive()` + `Clock::nowMilliseconds()`), matching `ComboSetupModeTrigger`'s style — not the sibling project's `Level`/`Instant`/`Duration` types.
- The BOOT button (GPIO0) is read live during `loop()`, well after boot has completed — never as a boot-time strapping-pin read (holding GPIO0 low through power-on/reset puts the ESP32 into UART download mode instead of running application code).
- `setIdentifying(true)` is already idempotent (`lib/McsEsp32/src/domain/LedPairDriver.cpp`) — safe to call unconditionally every `loop()` tick, matching how the existing MQTT identify-blink path already calls it.
- Delete `ComboSetupModeTrigger` and its test suite only after nothing in `src/esp32/main.cpp` references it — Task 3 must run after Task 2, never before.
- `SETUP_TRIGGER_HOLD_MS` (3000ms) in `src/esp32/main.cpp` is unchanged.

---

### Task 1: `ButtonSetupModeTrigger`

**Files:**
- Create: `lib/McsEsp32/src/adapters/ButtonSetupModeTrigger.h`
- Create: `lib/McsEsp32/src/adapters/ButtonSetupModeTrigger.cpp`
- Test: `test/test_button_setup_mode_trigger/test_main.cpp`

**Interfaces:**
- Consumes: `DigitalInput` (`lib/McsCore/src/ports/DigitalInput.h`) — `virtual bool isActive() const = 0;`. `Clock` (`lib/McsCore/src/ports/Clock.h`) — `virtual unsigned long nowMilliseconds() const = 0;`. `FakeDigitalInput`/`FakeClock` (`test/support/`) — `FakeDigitalInput` has a public `bool active` field; `FakeClock` has `void advanceBy(unsigned long)`.
- Produces (consumed by Task 2):
  ```cpp
  class ButtonSetupModeTrigger
  {
  public:
      ButtonSetupModeTrigger(DigitalInput& button, Clock& clock, unsigned long minHoldMs);
      void update();
      [[nodiscard]] bool requested() const;
  };
  ```

- [ ] **Step 1: Write the failing test file**

Create `test/test_button_setup_mode_trigger/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "adapters/ButtonSetupModeTrigger.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"

namespace
{
    constexpr unsigned long MIN_HOLD_MS = 3000;
}

TEST_CASE("requested is false initially and while the button is merely held, not yet released")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    trigger.update();
    REQUIRE_FALSE(trigger.requested());

    button.active = true;
    trigger.update();
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("requested stays false if released before minHoldMs elapses")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS - 1);
    button.active = false;
    trigger.update();

    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("requested fires exactly once on the release tick after meeting minHoldMs, then resets")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();

    clock.advanceBy(MIN_HOLD_MS);
    button.active = false;
    trigger.update();

    REQUIRE(trigger.requested());

    trigger.update();
    REQUIRE_FALSE(trigger.requested());
}

TEST_CASE("the hold survives intermediate update ticks without restarting the timer")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();

    for (int i = 0; i < 10; ++i)
    {
        clock.advanceBy(MIN_HOLD_MS / 10);
        trigger.update();
        REQUIRE_FALSE(trigger.requested());
    }

    button.active = false;
    trigger.update();

    REQUIRE(trigger.requested());
}

TEST_CASE("a fresh press-hold-release cycle after a full release can trigger again")
{
    FakeDigitalInput button;
    FakeClock clock;
    ButtonSetupModeTrigger trigger(button, clock, MIN_HOLD_MS);

    button.active = true;
    trigger.update();
    clock.advanceBy(MIN_HOLD_MS);
    button.active = false;
    trigger.update();
    REQUIRE(trigger.requested());

    trigger.update();
    REQUIRE_FALSE(trigger.requested());

    button.active = true;
    trigger.update();
    clock.advanceBy(MIN_HOLD_MS);
    button.active = false;
    trigger.update();

    REQUIRE(trigger.requested());
}
```

- [ ] **Step 2: Run the test to verify it fails to compile**

Run: `pio test -e native -f test_button_setup_mode_trigger`
Expected: FAIL — `ButtonSetupModeTrigger.h` does not exist yet.

- [ ] **Step 3: Write the header**

Create `lib/McsEsp32/src/adapters/ButtonSetupModeTrigger.h`:

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

- [ ] **Step 4: Write the implementation**

Create `lib/McsEsp32/src/adapters/ButtonSetupModeTrigger.cpp`:

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

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_button_setup_mode_trigger`
Expected: PASS — 5 test cases, 0 failures.

- [ ] **Step 6: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites pass (41/41 — 40 existing plus this task's new suite; `ComboSetupModeTrigger`'s suite is still present at this point, removed in Task 3).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/adapters/ButtonSetupModeTrigger.h lib/McsEsp32/src/adapters/ButtonSetupModeTrigger.cpp test/test_button_setup_mode_trigger/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add ButtonSetupModeTrigger

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CGYUBEe91zFYStbvaxPD3c
EOF
)"
```

---

### Task 2: Wire the BOOT button and setup-mode LED flash into `src/esp32/main.cpp`

**Files:**
- Modify: `src/esp32/main.cpp`

**Interfaces:**
- Consumes: `ButtonSetupModeTrigger` (Task 1) — `ButtonSetupModeTrigger(DigitalInput&, Clock&, unsigned long)`, `void update()`, `bool requested() const`. `ArduinoDigitalInput` (`lib/McsCore/src/adapters/ArduinoDigitalInput.h`) — `ArduinoDigitalInput(int pin, bool activeLow, bool useInternalPullup)`, `void begin()`. `LedPairStation::setIdentifying(bool)` (existing, sub-project #2d-b) and `LedPairStation::update()` (existing).
- Produces: nothing — this is the composition root, nothing else depends on it.

This task has no dedicated native test (matches existing convention for `main.cpp` — verified via `pio run -e esp32dev` build success and a manual read-through, same as every other composition-root change to this file).

- [ ] **Step 1: Swap the include**

In `src/esp32/main.cpp`, find:

```cpp
#include "adapters/ComboSetupModeTrigger.h"
```

Replace with:

```cpp
#include "adapters/ButtonSetupModeTrigger.h"
```

- [ ] **Step 2: Add the `bootButton` global**

Find:

```cpp
MatrixScanner matrixScanner({&matrixRow0, &matrixRow1, &matrixRow2},
                            {&matrixCol0, &matrixCol1, &matrixCol2, &matrixCol3});
```

Replace with:

```cpp
MatrixScanner matrixScanner({&matrixRow0, &matrixRow1, &matrixRow2},
                            {&matrixCol0, &matrixCol1, &matrixCol2, &matrixCol3});

ArduinoDigitalInput bootButton(0, /*activeLow=*/true, /*useInternalPullup=*/true);
```

- [ ] **Step 3: `begin()` the new input**

Find:

```cpp
    matrixCol0.begin();
    matrixCol1.begin();
    matrixCol2.begin();
    matrixCol3.begin();
```

Replace with:

```cpp
    matrixCol0.begin();
    matrixCol1.begin();
    matrixCol2.begin();
    matrixCol3.begin();
    bootButton.begin();
```

- [ ] **Step 4: Swap the trigger construction**

Find:

```cpp
ComboSetupModeTrigger setupTrigger(matrixButtons[0], matrixButtons[1], systemClock, SETUP_TRIGGER_HOLD_MS);
```

Replace with:

```cpp
ButtonSetupModeTrigger setupTrigger(bootButton, systemClock, SETUP_TRIGGER_HOLD_MS);
```

- [ ] **Step 5: Simplify the per-tick suppression loop**

Find:

```cpp
    gatedButtons[0].setSuppressed(setupTrigger.isHolding() || collision);
    gatedButtons[1].setSuppressed(setupTrigger.isHolding() || collision);
    for (int i = 2; i < 12; ++i)
    {
        gatedButtons[i].setSuppressed(collision);
    }
```

Replace with:

```cpp
    for (auto& gated : gatedButtons)
    {
        gated.setSuppressed(collision);
    }
```

- [ ] **Step 6: Flash all 12 LED pairs while wireless setup is active**

Find, in `loop()`:

```cpp
    if (bootMode == BootMode::WirelessSetup)
    {
        captivePortalServer.poll();
        return;
    }
```

Replace with:

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

- [ ] **Step 7: Build-check the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 8: Run the full native suite to check for regressions**

Run: `pio test -e native`
Expected: All suites still pass (41/41, unchanged from Task 1 — `main.cpp` isn't part of the `native` build).

- [ ] **Step 9: Commit**

```bash
git add src/esp32/main.cpp
git commit -m "$(cat <<'EOF'
! F Wire BOOT-button setup trigger and wireless-setup LED flash into ESP32 main

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CGYUBEe91zFYStbvaxPD3c
EOF
)"
```

---

### Task 3: Remove `ComboSetupModeTrigger`

**Files:**
- Delete: `lib/McsEsp32/src/adapters/ComboSetupModeTrigger.h`
- Delete: `lib/McsEsp32/src/adapters/ComboSetupModeTrigger.cpp`
- Delete: `test/test_combo_setup_mode_trigger/test_main.cpp` (and the now-empty `test/test_combo_setup_mode_trigger/` directory)

**Interfaces:** none — this task only removes code Task 2 already stopped referencing.

- [ ] **Step 1: Confirm nothing still references it**

Run: `grep -rl "ComboSetupModeTrigger" src/ lib/ test/`
Expected: only the three paths being deleted in this task — `lib/McsEsp32/src/adapters/ComboSetupModeTrigger.h`, `lib/McsEsp32/src/adapters/ComboSetupModeTrigger.cpp`, `test/test_combo_setup_mode_trigger/test_main.cpp`. If `src/esp32/main.cpp` still appears, stop — Task 2 wasn't completed correctly.

- [ ] **Step 2: Delete the files**

```bash
git rm lib/McsEsp32/src/adapters/ComboSetupModeTrigger.h lib/McsEsp32/src/adapters/ComboSetupModeTrigger.cpp test/test_combo_setup_mode_trigger/test_main.cpp
```

- [ ] **Step 3: Run the full native suite to confirm the deletion is clean**

Run: `pio test -e native`
Expected: All suites pass (40/40 — back down from 41, since `ComboSetupModeTrigger`'s suite is gone and nothing else changed since Task 1).

- [ ] **Step 4: Build-check the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — confirms nothing in the hardware build referenced the deleted files either.

- [ ] **Step 5: Commit**

```bash
git add -u lib/McsEsp32/src/adapters/ComboSetupModeTrigger.h lib/McsEsp32/src/adapters/ComboSetupModeTrigger.cpp test/test_combo_setup_mode_trigger/test_main.cpp
git commit -m "$(cat <<'EOF'
. r Remove ComboSetupModeTrigger, superseded by ButtonSetupModeTrigger

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CGYUBEe91zFYStbvaxPD3c
EOF
)"
```

---

### Task 4: Update documentation

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/ESP32_Turnout_Panel_Implementation.md`
- Modify: `docs/HARDWARE_BRINGUP_CHECKLIST.md`
- Modify: `docs/superpowers/specs/2026-08-29-esp32-wireless-setup-trigger-design.md`

No test — documentation only.

- [ ] **Step 1: `CLAUDE.md` — update the `ComboSetupModeTrigger` adapter description**

Find (end of the McsEsp32 `adapters/` bullet):

```
ComboSetupModeTrigger (detects two `DigitalInput`s held simultaneously for a minimum duration then either releasing, edge-triggered like `Button::wasPressed()`; wired in `src/esp32/main.cpp` against the raw T1/T2 `MatrixDigitalInput`s in sub-project #2c-b2 — a documented, deliberately-accepted gap remains where a human's staggered two-finger press can let the earlier-pressed button fire one ordinary toggle command before suppression engages, see the design spec's "Known gap" section)
```

Replace with:

```
ButtonSetupModeTrigger (detects the ESP32 board's own BOOT button (GPIO0) held for a minimum duration then released, edge-triggered like `Button::wasPressed()`; wired in `src/esp32/main.cpp` against a dedicated `bootButton` input — sub-project #2c-c replaced the earlier T1+T2 `ComboSetupModeTrigger` combo with this dedicated-button gesture, which needs no new wiring since the BOOT button is already present on the board, and eliminates the staggered-two-finger-press gap the combo had)
```

- [ ] **Step 2: `CLAUDE.md` — update the `main.cpp` bullet's `GatedDigitalInput` parenthetical**

Find:

```
(each wrapped in a `GatedDigitalInput` before being handed to its station, so a held T1+T2 combo can suppress those two buttons' normal toggle command)
```

Replace with:

```
(each wrapped in a `GatedDigitalInput` before being handed to its station, so a latched `nodeId` collision can suppress all 12 buttons' normal toggle command)
```

- [ ] **Step 3: `CLAUDE.md` — update the `ComboSetupModeTrigger` sentence in the `main.cpp` bullet**

Find:

```
In that path, a `ComboSetupModeTrigger` reads the *raw* T1/T2 `MatrixDigitalInput`s every `loop()` tick (never the gated wrappers — wiring it to gated inputs would deadlock the gesture) and, on a 3-second hold-then-release, calls `NvsSetupModeRequestStore::requestOnNextBoot()` and `ESP.restart()`
```

Replace with:

```
In that path, a `ButtonSetupModeTrigger` reads the *raw* `bootButton` input (GPIO0, the ESP32 board's own BOOT button, independent of the 12-button matrix) every `loop()` tick and, on a 3-second hold-then-release, calls `NvsSetupModeRequestStore::requestOnNextBoot()` and `ESP.restart()`
```

- [ ] **Step 4: `CLAUDE.md` — update the suppression sentence and add the wireless-setup LED-flash sentence**

Find:

```
suppresses all 12 `gatedButtons` (T1/T2 composed with the existing combo-hold suppression via `||`, the other 10 driven by `collision` alone), and folds `!collision` into the same `clearIndicator()` gate as `configValid`/MQTT-connected.
```

Replace with:

```
suppresses all 12 `gatedButtons` uniformly, driven by `collision` alone (no combo-hold special-casing remains now that the setup gesture uses the independent `bootButton` rather than two of the 12 turnout buttons), and folds `!collision` into the same `clearIndicator()` gate as `configValid`/MQTT-connected. **Sub-project #2c-c** also adds LED feedback for wireless setup mode itself: the `BootMode::WirelessSetup` branch of `loop()` calls `ledStation.setIdentifying(true)` and `ledStation.update()` on all 12 `LedPairStation`s every tick, reusing the #2d-b identify-blink override so all 12 LED pairs flash for as long as the setup AP is open — the two states can never overlap (wireless setup mode never starts MQTT, so the MQTT-triggered identify-blink can't fire at the same time), so sharing the exact visual is unambiguous in practice.
```

- [ ] **Step 5: `CLAUDE.md` — close out the "gap remains open" paragraph**

Find:

```
One gap remains open by explicit, documented decision rather than
oversight: `docs/superpowers/specs/2026-08-29-esp32-wireless-setup-trigger-design.md`'s
staggered-press gap — a human's two-finger T1+T2 press can let the
earlier-pressed button fire one ordinary toggle command before the combo's
suppression engages. #2c-b2 chose to accept this (self-correcting, rare,
avoids adding latency/complexity to every other button's normal press)
rather than close it with a hold-off timer or a command-on-release change.
```

Replace with:

```
The staggered-press gap #2c-b2 had explicitly accepted (a human's
two-finger T1+T2 press could fire one ordinary toggle command on
whichever button was pressed first, before the combo's suppression
engaged) no longer exists: sub-project #2c-c
(`docs/superpowers/specs/2026-09-01-esp32-boot-button-setup-trigger-design.md`)
replaced the T1+T2 combo with a single dedicated-button gesture on the
ESP32's own BOOT button (GPIO0), which has no analogous two-input race
since there's only one input to hold.
```

- [ ] **Step 6: `CLAUDE.md` — update the collision-lockout escape-hatch reference**

Find:

```
Serial commissioning and the T1+T2
wireless-setup combo (wired to the *raw*, ungated matrix inputs — see the
`ComboSetupModeTrigger` entry above) both keep working during a collision
lockout, so recovery never requires broker administration.
```

Replace with:

```
Serial commissioning and the BOOT-button wireless-setup gesture (wired to
the *raw* `bootButton` input, independent of the gated turnout matrix —
see the `ButtonSetupModeTrigger` entry above) both keep working during a
collision lockout, so recovery never requires broker administration.
```

- [ ] **Step 7: `docs/ESP32_Turnout_Panel_Implementation.md` — update the "Wireless Setup Access Point" trigger description**

Find:

```
Wireless setup mode is entered by holding turnout buttons T1+T2 together for
3 seconds — this works the same way whether the panel is a factory-fresh
board that has never been configured, or an already-commissioned panel a
technician wants to reconfigure. A factory-fresh panel does **not**
automatically open the AP on its own; without the T1+T2 hold it boots
waiting for bench-serial commissioning instead (see "JMRI Communication
(MQTT)" and the bench-serial commissioning design for that path).
```

Replace with:

```
Wireless setup mode is entered by holding the ESP32 board's own BOOT
button (GPIO0) for 3 seconds, then releasing it — this works the same way
whether the panel is a factory-fresh board that has never been
configured, or an already-commissioned panel a technician wants to
reconfigure. No extra wiring is needed; the BOOT button is already
present on the board. A factory-fresh panel does **not** automatically
open the AP on its own; without the BOOT-button hold it boots waiting for
bench-serial commissioning instead (see "JMRI Communication (MQTT)" and
the bench-serial commissioning design for that path).

While the setup AP is open, all 12 LED pairs flash green/red together at
the same fast rate used for MQTT identify-blink (`LedPairDriver::setIdentifying()`,
sub-project #2d-b) — the two states can never overlap, since wireless
setup mode never starts MQTT.
```

- [ ] **Step 8: `docs/ESP32_Turnout_Panel_Implementation.md` — update the collision-lockout escape-hatch bullet**

Find:

```
- **The T1+T2 wireless-setup combo** (hold both buttons 3 seconds) — still
  opens the wireless setup AP even during a lockout, since it reads the
  same underlying buttons before the lockout's suppression is applied.
```

Replace with:

```
- **The BOOT-button wireless-setup gesture** (hold 3 seconds, then
  release) — still opens the wireless setup AP even during a lockout,
  since it reads a dedicated input (GPIO0) that collision suppression
  never touches.
```

- [ ] **Step 9: `docs/HARDWARE_BRINGUP_CHECKLIST.md` — update section 2.1's prerequisites bullet**

Find:

```
- The ELEGOO ESP32 board wired per `docs/ESP32_Turnout_Panel_Implementation.md`'s
  GPIO Assignment section (3×4 button matrix, 12 LED pairs) — at minimum,
  wire T1 and T2 fully (needed for the wireless-setup combo trigger you'll
  use in 2.3) plus however many additional turnouts you're bringing up.
```

Replace with:

```
- The ELEGOO ESP32 board wired per `docs/ESP32_Turnout_Panel_Implementation.md`'s
  GPIO Assignment section (3×4 button matrix, 12 LED pairs) — wire
  however many turnouts you're bringing up. The wireless-setup gesture
  (2.4) uses the board's own onboard BOOT button, not any turnout wiring,
  so no turnout needs to be wired first just to exercise it.
```

- [ ] **Step 10: `docs/HARDWARE_BRINGUP_CHECKLIST.md` — update section 2.4's trigger steps**

Find:

```
1. Hold **T1 and T2 together for 3 seconds**, then release. Serial should
   log `"Entering wireless setup..."` and the panel reboots.
2. On a phone or laptop, look for a WiFi network named `MaltBee-Setup-XXXX`
   (last 4 hex digits of the chip's MAC). Join it using the passphrase
   `maltbee-setup` (documented in `docs/ESP32_Turnout_Panel_Implementation.md`'s
   "Wireless Setup Access Point" section).
```

Replace with:

```
1. Hold the ESP32 board's **BOOT button for 3 seconds**, then release.
   Serial should log `"Entering wireless setup..."` and the panel reboots.
2. Once it reboots into wireless setup mode, confirm all 12 LED pairs
   flash green/red together — this is the same fast flash MQTT
   identify-blink uses (`docs/ESP32_Turnout_Panel_Implementation.md`'s
   "Identifying a Physical Panel" section), reused here since the two
   states can never overlap.
3. On a phone or laptop, look for a WiFi network named `MaltBee-Setup-XXXX`
   (last 4 hex digits of the chip's MAC). Join it using the passphrase
   `maltbee-setup` (documented in `docs/ESP32_Turnout_Panel_Implementation.md`'s
   "Wireless Setup Access Point" section).
```

- [ ] **Step 11: `docs/HARDWARE_BRINGUP_CHECKLIST.md` — renumber the remaining 2.4 steps and drop the resolved "known gap" note**

Find:

```
3. A captive-portal prompt should appear automatically (or navigate to
   any HTTP address — all DNS is redirected). Confirm the form pre-fills
   already-commissioned values (node ID, SSID, channel names) — but never
   the real WiFi password, which should always show blank.
4. Submit a change (e.g. a new channel name) and confirm the panel reboots
   and applies it.
5. Confirm normal turnout operation and buttons are completely unaffected
   by being in this mode — while the AP is up, turnout control is
   intentionally suspended (see `BootMode::WirelessSetup` in
   `src/esp32/main.cpp`); confirm it resumes normally after the reboot in
   step 4.

**Known accepted gap to expect, not a bug:** pressing T1 then T2 with any
noticeable stagger between them may fire one ordinary toggle command on
whichever was pressed first, before the combo is recognized. This is
documented and intentional (see `docs/superpowers/specs/2026-08-29-esp32-wireless-setup-trigger-design.md`) — it's self-correcting, not something to debug.
```

Replace with:

```
4. A captive-portal prompt should appear automatically (or navigate to
   any HTTP address — all DNS is redirected). Confirm the form pre-fills
   already-commissioned values (node ID, SSID, channel names) — but never
   the real WiFi password, which should always show blank.
5. Submit a change (e.g. a new channel name) and confirm the panel reboots
   and applies it.
6. Confirm normal turnout operation and buttons are completely unaffected
   by being in this mode — while the AP is up, turnout control is
   intentionally suspended (see `BootMode::WirelessSetup` in
   `src/esp32/main.cpp`); confirm it resumes normally after the reboot in
   step 5, and confirm the LED flash from step 2 stops at the same time.
```

- [ ] **Step 12: Add a superseded-by pointer to the old trigger spec**

Find the very top of `docs/superpowers/specs/2026-08-29-esp32-wireless-setup-trigger-design.md`:

```
# ESP32 Wireless Setup Boot Mode & Trigger (Sub-project #2c-a) — Design
```

Replace with:

```
# ESP32 Wireless Setup Boot Mode & Trigger (Sub-project #2c-a) — Design

**Superseded (2026-09-01):** the T1+T2 combo trigger this spec designed
(`ComboSetupModeTrigger`) was replaced by a single-button gesture on the
ESP32's own BOOT button — see
`docs/superpowers/specs/2026-09-01-esp32-boot-button-setup-trigger-design.md`.
`BootMode`/`BootModeSelector`/`SetupModeRequestStore`/`GatedDigitalInput`,
also designed here, are unaffected and still current. Kept for history,
not deleted.
```

- [ ] **Step 13: Commit**

```bash
git add CLAUDE.md docs/ESP32_Turnout_Panel_Implementation.md docs/HARDWARE_BRINGUP_CHECKLIST.md docs/superpowers/specs/2026-08-29-esp32-wireless-setup-trigger-design.md
git commit -m "$(cat <<'EOF'
. d Update ESP32 docs for BOOT-button wireless setup trigger

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CGYUBEe91zFYStbvaxPD3c
EOF
)"
```

---

## Self-review notes

- **Spec coverage:** All four Decisions from `docs/superpowers/specs/2026-09-01-esp32-boot-button-setup-trigger-design.md` are covered: Decision 1 (BOOT button, ≥3s hold, fires on release) → Task 1 + Task 2 Steps 2-4; Decision 2 (reuse `setIdentifying()`) → Task 2 Step 6; Decision 3 (delete, not deprecate, `ComboSetupModeTrigger`) → Task 3; Decision 4 (uniform collision-only suppression) → Task 2 Step 5. The spec's full File Layout list (new trigger + test, `main.cpp` changes, deleted files, `CLAUDE.md`/`HARDWARE_BRINGUP_CHECKLIST.md`/old-spec doc updates) matches Tasks 1-4 exactly. The spec's Non-goals (no `LedPairDriver`/`LedPairStation`/`IdentifyModeTimer` changes, no distinct visual pattern, no `NvsSetupModeRequestStore`/`BootModeSelector` changes, no `isHolding()` accessor) are respected — no task touches any of those.
- **Placeholder scan:** No TBD/TODO; every step has complete, concrete code or exact find/replace text.
- **Type consistency:** `ButtonSetupModeTrigger`'s constructor `(DigitalInput&, Clock&, unsigned long)` and its `update()`/`requested()` method names, defined in Task 1, are used identically in Task 2 Step 4. `bootButton`'s type (`ArduinoDigitalInput`) and construction in Task 2 Step 2 matches how it's used in Steps 3-4. No task references `ComboSetupModeTrigger::isHolding()` anywhere after Task 2 Step 5 removes its call site.
