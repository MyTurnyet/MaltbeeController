# ESP32 Single-Button Toggle Turnout Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `ToggleTurnoutControl`, a new ESP32-only application class that lets one matrix button (instead of the Mega's two physical buttons) drive a turnout by toggling to the opposite of its last-confirmed position, without changing the existing `TurnoutControl`.

**Architecture:** One new class, `ToggleTurnoutControl`, in `lib/McsEsp32/src/application/`, built via TDD against a single native test file covering every behavior from the design spec. No other file changes — `TurnoutControl`, `Button`, `Turnout`, `TurnoutIndicator`, and `TurnoutCommandPort` are all reused unmodified via rooted cross-library includes, the same convention already used elsewhere in `lib/McsEsp32`.

**Tech Stack:** C++17, PlatformIO `native` environment, Catch2 test framework.

## Global Constraints

- Full design source of truth: `docs/superpowers/specs/2026-08-29-esp32-toggle-turnout-control-design.md`.
- `ToggleTurnoutControl` lives in `lib/McsEsp32/src/application/`, not `lib/McsCore/` — this is ESP32-only logic (the Mega's two-button wiring never needs it).
- Do not modify `TurnoutControl` (`lib/McsCore/src/application/TurnoutControl.h`/`.cpp`), `Button`, `Turnout`, `TurnoutIndicator`, or `TurnoutCommandPort` in any way.
- A press computes the opposite of `turnout_.position()` (the last JMRI-confirmed position, or the turnout's constructed initial position before any feedback has arrived) and sends that. Do **not** add a separate locally-tracked "last commanded" field — repeated presses before confirmation must resend the identical command each time, not alternate.
- No `Turnout::isLocked()`/`isDisabled()` checks — `update()` behaves at parity with `TurnoutControl::update()`, which doesn't check these either.
- Cross-library includes from `lib/McsEsp32/src/application/ToggleTurnoutControl.h` into `lib/McsCore` use rooted includes (`"domain/Button.h"`, `"domain/Turnout.h"`, `"domain/TurnoutIndicator.h"`, `"ports/TurnoutCommandPort.h"`), matching the convention already used by `lib/McsEsp32/src/adapters/JmriTurnoutCommandAdapter.h` (`"ports/TurnoutCommandPort.h"`) and `lib/McsEsp32/src/domain/PayloadCodec.h` (`"domain/Turnout.h"`).
- Commit messages use this project's Arlo's Commit Notation (ACN) per `CLAUDE.md`.
- After this task, `pio test -e native` must show one additional passing suite (28 total) with zero regressions, and `pio run -e esp32dev`/`pio run -e megaatmega2560` must both still succeed (this task adds no Arduino-guarded code, so both should be trivially unaffected).

---

### Task 1: `ToggleTurnoutControl`

**Files:**
- Create: `lib/McsEsp32/src/application/ToggleTurnoutControl.h`
- Create: `lib/McsEsp32/src/application/ToggleTurnoutControl.cpp`
- Create: `test/test_toggle_turnout_control/test_main.cpp`

**Interfaces:**
- Consumes: `Button { bool wasPressed() const; void update(); }` (`lib/McsCore/src/domain/Button.h`); `Turnout { TurnoutPosition position() const; int address() const; void throwDiverging(); void throwStraight(); }` (`lib/McsCore/src/domain/Turnout.h`); `TurnoutIndicator { void display(TurnoutPosition); }` (`lib/McsCore/src/domain/TurnoutIndicator.h`); `TurnoutCommandPort { void send(int address, TurnoutPosition position); }` and `struct TurnoutFeedback { int address; TurnoutPosition position; }` (`lib/McsCore/src/ports/TurnoutCommandPort.h`).
- Produces: `class ToggleTurnoutControl` with constructor `ToggleTurnoutControl(Button& button, Turnout& turnout, TurnoutIndicator& indicator, TurnoutCommandPort& turnoutCommandPort)`, plus `void update()` and `void applyFeedback(TurnoutFeedback feedback)`. Sub-project #7b will construct 12 of these (one per turnout) instead of 12 `TurnoutControl`s.

- [ ] **Step 1: Write the failing test file**

Create `test/test_toggle_turnout_control/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/ToggleTurnoutControl.h"
#include "domain/Button.h"
#include "domain/Indicator.h"
#include "domain/Turnout.h"
#include "domain/TurnoutIndicator.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeDigitalOutput.h"
#include "support/FakeTurnoutCommandPort.h"

namespace
{
    constexpr unsigned long DEBOUNCE_MS = 30;

    void pressButton(Button& button, FakeDigitalInput& input, FakeClock& clock)
    {
        input.active = true;
        button.update();
        clock.advanceBy(DEBOUNCE_MS);
        button.update();
    }
}

TEST_CASE("A press sends the opposite of the turnout's constructed-default position")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    pressButton(button, buttonInput, clock);
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
    REQUIRE(commandPort.sentCommands[0].address == 101);
    REQUIRE(commandPort.sentCommands[0].position == TurnoutPosition::Thrown);
}

TEST_CASE("After feedback confirms a position, the next press sends the opposite of that confirmed position")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({101, TurnoutPosition::Thrown});

    pressButton(button, buttonInput, clock);
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
    REQUIRE(commandPort.sentCommands[0].position == TurnoutPosition::Closed);
}

TEST_CASE("Repeated presses with no intervening feedback send the identical command each time")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    pressButton(button, buttonInput, clock);
    control.update();

    buttonInput.active = false;
    button.update();
    pressButton(button, buttonInput, clock);
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 2);
    REQUIRE(commandPort.sentCommands[0].position == TurnoutPosition::Thrown);
    REQUIRE(commandPort.sentCommands[1].position == TurnoutPosition::Thrown);
}

TEST_CASE("Holding the button sends no repeat commands")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    pressButton(button, buttonInput, clock);
    control.update();

    clock.advanceBy(DEBOUNCE_MS);
    button.update();
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
}

TEST_CASE("Feedback for a matching address updates the turnout position and indicator, closed to thrown")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({101, TurnoutPosition::Thrown});

    REQUIRE(turnout.position() == TurnoutPosition::Thrown);
    REQUIRE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
}

TEST_CASE("Feedback for a matching address updates the turnout position and indicator, thrown to closed")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Thrown, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({101, TurnoutPosition::Closed});

    REQUIRE(turnout.position() == TurnoutPosition::Closed);
    REQUIRE(closedIndicator.isOn());
    REQUIRE_FALSE(thrownIndicator.isOn());
}

TEST_CASE("Feedback for a non-matching address is ignored")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({202, TurnoutPosition::Thrown});

    REQUIRE(turnout.position() == TurnoutPosition::Closed);
    REQUIRE_FALSE(thrownIndicator.isOn());
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_toggle_turnout_control`
Expected: FAIL to compile — `application/ToggleTurnoutControl.h` does not exist yet.

- [ ] **Step 3: Write `ToggleTurnoutControl.h`**

Create `lib/McsEsp32/src/application/ToggleTurnoutControl.h`:

```cpp
#pragma once

#include "domain/Button.h"
#include "domain/Turnout.h"
#include "domain/TurnoutIndicator.h"
#include "ports/TurnoutCommandPort.h"

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

- [ ] **Step 4: Write `ToggleTurnoutControl.cpp`**

Create `lib/McsEsp32/src/application/ToggleTurnoutControl.cpp`:

```cpp
#include "ToggleTurnoutControl.h"

ToggleTurnoutControl::ToggleTurnoutControl(Button& button, Turnout& turnout, TurnoutIndicator& indicator,
                                            TurnoutCommandPort& turnoutCommandPort)
    : button_(button), turnout_(turnout), indicator_(indicator), turnoutCommandPort_(turnoutCommandPort)
{
}

void ToggleTurnoutControl::update()
{
    if (!button_.wasPressed())
    {
        return;
    }

    const TurnoutPosition opposite =
        turnout_.position() == TurnoutPosition::Closed ? TurnoutPosition::Thrown : TurnoutPosition::Closed;
    turnoutCommandPort_.send(turnout_.address(), opposite);
}

void ToggleTurnoutControl::applyFeedback(const TurnoutFeedback feedback)
{
    if (feedback.address != turnout_.address())
    {
        return;
    }

    if (feedback.position == TurnoutPosition::Thrown)
    {
        turnout_.throwDiverging();
    }
    else
    {
        turnout_.throwStraight();
    }

    indicator_.display(feedback.position);
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_toggle_turnout_control`
Expected: PASS, all 7 test cases green.

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every suite green — 28 suites total (27 existing + this new one).

- [ ] **Step 7: Build for the ESP32 and Mega targets to confirm both are unaffected**

Run: `pio run -e esp32dev`
Expected: SUCCESS.

Run: `pio run -e megaatmega2560`
Expected: SUCCESS.

- [ ] **Step 8: Commit**

```bash
git add lib/McsEsp32/src/application/ToggleTurnoutControl.h lib/McsEsp32/src/application/ToggleTurnoutControl.cpp test/test_toggle_turnout_control/test_main.cpp
git commit -m "^ F Add ToggleTurnoutControl for ESP32 single-button turnouts"
```

---

## Self-Review Notes

- **Spec coverage:** the constructor/`update()`/`applyFeedback()` shape, the "toggle against confirmed position, not a separate commanded-intent flag" decision, the deliberate duplication of `TurnoutControl::applyFeedback()`'s logic, and every listed test behavior (press-sends-opposite, post-feedback press, repeated-press idempotence, non-matching-address ignored, matching-address updates both directions, holding sends nothing) all map directly to Step 1's test file and Steps 3-4's implementation. The "no lock/disabled checks" non-goal is satisfied by omission — `update()` never calls `isLocked()`/`isDisabled()`.
- **Placeholder scan:** none — every step has complete, literal code and exact commands.
- **Type consistency:** `ToggleTurnoutControl`'s constructor signature and `update()`/`applyFeedback(TurnoutFeedback)` are declared identically between the header (Step 3) and how the test file (Step 1) and implementation (Step 4) use them.
