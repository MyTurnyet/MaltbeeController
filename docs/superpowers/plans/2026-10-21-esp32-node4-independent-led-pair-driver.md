# Independent-Output Turnout Indicator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a driver and output-wrapper pair that let the node board's two independent per-turnout LED GPIOs honor the existing `Indicator`/`TurnoutIndicator` calls, reproducing the 12-channel panel's exact red/green blink for the unconfirmed state.

**Architecture:** `IndependentLedPairDriver` (domain) is a direct logic port of the existing `LedPairDriver` — same `Mode`/blink/identify state machine, reusing the `LedPairColor` enum — but its `writeColor()` drives two real `DigitalOutput&`s instead of one shared pin, always keeping them mutually exclusive. `IndependentLedPairOutput` (adapter) mirrors `LedPairOutput` exactly, forwarding `DigitalOutput::set()`/`isSet()` to the new driver.

**Tech Stack:** C++17, Catch2 (native tests), the project's existing `test/support/FakeDigitalOutput.h`/`FakeClock.h` test doubles.

## Global Constraints

- `pio test -e native` must pass all pre-existing suites unchanged, plus the two new suites this plan adds.
- `LedPairDriver`, `LedPairOutput`, `Indicator`, `TurnoutIndicator`, `TurnoutControl`, and `TurnoutPosition` must not be modified.
- No GPIO-owning composition-helper class, no real `ArduinoDigitalOutput` wiring, no `main.cpp` changes — out of scope per the spec.
- Full spec: `docs/superpowers/specs/2026-10-21-esp32-node4-independent-led-pair-driver-design.md`.

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `lib/McsEsp32/src/domain/IndependentLedPairDriver.h` | Create | Mode/blink/identify state machine driving two real GPIOs |
| `lib/McsEsp32/src/domain/IndependentLedPairDriver.cpp` | Create | Implementation |
| `test/test_independent_led_pair_driver/test_main.cpp` | Create | 17 cases mirroring `test_led_pair_driver` |
| `lib/McsEsp32/src/adapters/IndependentLedPairOutput.h` | Create | `DigitalOutput` forwarding to one side of the driver |
| `lib/McsEsp32/src/adapters/IndependentLedPairOutput.cpp` | Create | Implementation |
| `test/test_independent_led_pair_output/test_main.cpp` | Create | 3 cases mirroring `test_led_pair_output` |

---

## Task 1: `IndependentLedPairDriver`

**Files:**
- Create: `lib/McsEsp32/src/domain/IndependentLedPairDriver.h`
- Create: `lib/McsEsp32/src/domain/IndependentLedPairDriver.cpp`
- Test: `test/test_independent_led_pair_driver/test_main.cpp`

**Interfaces:**
- Consumes: `LedPairColor` (reused from `lib/McsEsp32/src/domain/LedPairDriver.h`, not duplicated), `DigitalOutput`/`Clock` ports (`lib/McsCore/src/ports/`).
- Produces: `IndependentLedPairDriver(DigitalOutput& greenGpio, DigitalOutput& redGpio, Clock& clock, unsigned long blinkIntervalMs, LedPairColor defaultColor)`; `begin()`, `setGreen(bool)`, `setRed(bool)`, `isGreenRequested() const`, `isRedRequested() const`, `setIdentifying(bool)`, `update()` — same names/signatures as `LedPairDriver`, consumed by Task 2.

- [ ] **Step 1: Write the failing tests**

Create `test/test_independent_led_pair_driver/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/IndependentLedPairDriver.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalOutput.h"

namespace
{
    constexpr unsigned long BLINK_INTERVAL_MS = 100;
}

TEST_CASE("requesting green writes the green GPIO HIGH and the red GPIO LOW immediately")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setGreen(true);

    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}

TEST_CASE("requesting red writes the red GPIO HIGH and the green GPIO LOW immediately")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    driver.setRed(true);

    REQUIRE(redGpio.isSet());
    REQUIRE_FALSE(greenGpio.isSet());
}

TEST_CASE("before anything has been requested, both GPIOs already show the configured default color")
{
    FakeDigitalOutput greenDefaultGreenGpio;
    FakeDigitalOutput greenDefaultRedGpio;
    FakeClock clock;
    IndependentLedPairDriver greenDefaultDriver(greenDefaultGreenGpio, greenDefaultRedGpio, clock,
                                                 BLINK_INTERVAL_MS, LedPairColor::Green);

    REQUIRE(greenDefaultGreenGpio.isSet());
    REQUIRE_FALSE(greenDefaultRedGpio.isSet());
    REQUIRE(greenDefaultGreenGpio.setCallCount() == 1);
    REQUIRE(greenDefaultRedGpio.setCallCount() == 1);

    FakeDigitalOutput redDefaultGreenGpio;
    FakeDigitalOutput redDefaultRedGpio;
    IndependentLedPairDriver redDefaultDriver(redDefaultGreenGpio, redDefaultRedGpio, clock,
                                               BLINK_INTERVAL_MS, LedPairColor::Red);

    REQUIRE_FALSE(redDefaultGreenGpio.isSet());
    REQUIRE(redDefaultRedGpio.isSet());
    REQUIRE(redDefaultGreenGpio.setCallCount() == 1);
    REQUIRE(redDefaultRedGpio.setCallCount() == 1);
}

TEST_CASE("requesting both off enters blink and immediately shows the last-displayed color, from green")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setGreen(true);
    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());

    driver.setGreen(false);

    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}

TEST_CASE("requesting both off enters blink and immediately shows the last-displayed color, from red")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    driver.setRed(true);
    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());

    driver.setRed(false);

    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());
}

TEST_CASE("update() before the blink interval elapses does nothing")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    clock.advanceBy(BLINK_INTERVAL_MS - 1);
    driver.update();

    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}

TEST_CASE("update() after the blink interval elapses flips to the opposite color and resets the timer")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    clock.advanceBy(BLINK_INTERVAL_MS);
    driver.update();
    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());

    clock.advanceBy(BLINK_INTERVAL_MS);
    driver.update();
    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}

TEST_CASE("update() never touches either GPIO while a steady color is requested")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setGreen(true);
    clock.advanceBy(BLINK_INTERVAL_MS * 10);
    driver.update();

    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}

TEST_CASE("a redundant call that does not change the derived mode does not reset the blink timer")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    clock.advanceBy(BLINK_INTERVAL_MS - 1);
    driver.setGreen(false);
    driver.setRed(false);

    clock.advanceBy(1);
    driver.update();

    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());
}

TEST_CASE("requesting both colors simultaneously leaves both GPIOs exactly as they were")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setGreen(true);
    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());

    driver.setRed(true);
    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());

    driver.setGreen(false);

    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());
}

TEST_CASE("isGreenRequested and isRedRequested reflect the last request for each side independently")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    driver.setRed(true);

    REQUIRE(driver.isRedRequested());
    REQUIRE_FALSE(driver.isGreenRequested());
}

TEST_CASE("begin() re-writes the current color to both GPIOs and re-stamps the blink timer, for use after the GPIOs' own begin()")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    // Simulate real ArduinoDigitalOutputs' begin() happening after
    // construction, which on real hardware would clobber the GPIO levels
    // and leave the driver's internal timer referring to a clock time
    // before setup() ran.
    greenGpio.set(false);
    redGpio.set(true);
    clock.advanceBy(50);

    driver.begin();

    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());

    clock.advanceBy(BLINK_INTERVAL_MS - 1);
    driver.update();
    REQUIRE(greenGpio.isSet());

    clock.advanceBy(1);
    driver.update();
    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());
}

TEST_CASE("setIdentifying(true) immediately shows green on both GPIOs regardless of current mode")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setRed(true);
    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());

    driver.setIdentifying(true);

    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}

TEST_CASE("update() toggles the identify flash at its own fixed interval, independent of the blink interval")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setIdentifying(true);
    REQUIRE(greenGpio.isSet());

    clock.advanceBy(149);
    driver.update();
    REQUIRE(greenGpio.isSet());

    clock.advanceBy(1);
    driver.update();
    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());

    clock.advanceBy(150);
    driver.update();
    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}

TEST_CASE("setIdentifying(true) called again while already active does not reset the toggle timer")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setIdentifying(true);

    clock.advanceBy(149);
    driver.setIdentifying(true);
    driver.update();
    REQUIRE(greenGpio.isSet());

    clock.advanceBy(1);
    driver.update();
    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());
}

TEST_CASE("setIdentifying(false) reverts to a steady color the normal mode would show")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setRed(true);
    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());

    driver.setIdentifying(true);
    REQUIRE(greenGpio.isSet());

    driver.setIdentifying(false);

    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());
}

TEST_CASE("setIdentifying(false) reverts to blink mode's current color if that is what was active before")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    driver.setIdentifying(true);
    driver.setIdentifying(false);

    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());

    clock.advanceBy(BLINK_INTERVAL_MS);
    driver.update();
    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());
}

TEST_CASE("update() does not run normal blink logic while identifying")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    driver.setIdentifying(true);
    clock.advanceBy(BLINK_INTERVAL_MS);
    driver.update();

    // BLINK_INTERVAL_MS (100) has elapsed, which would have flipped the
    // GPIOs under the old blink logic — but the identify interval (150)
    // has not, so the short-circuit must leave them exactly as identify
    // set them (still green), proving the old blink path did not also run.
    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_independent_led_pair_driver`
Expected: FAIL to compile — `IndependentLedPairDriver` is not declared.

- [ ] **Step 3: Write `IndependentLedPairDriver.h`**

Create `lib/McsEsp32/src/domain/IndependentLedPairDriver.h`:

```cpp
#pragma once

#include "LedPairDriver.h" // for LedPairColor
#include "ports/Clock.h"
#include "ports/DigitalOutput.h"

// Drives two independent real GPIOs (green, red) with the same mode/blink
// state machine as LedPairDriver, which exists for a single shared pin
// that cannot go dark. These two GPIOs could show "both off" or "both on"
// (amber), but writeColor() deliberately never does either — it always
// drives exactly one high and the other low, reproducing the shared-pin
// driver's exact blink visual on independent hardware. update() must be
// called on every loop iteration for blinking to work.
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
    enum class Mode
    {
        Green,
        Red,
        Blink
    };

    void applyState();
    void writeColor(LedPairColor color);
    [[nodiscard]] LedPairColor currentColorToShow() const;

    DigitalOutput& greenGpio_;
    DigitalOutput& redGpio_;
    Clock& clock_;
    unsigned long blinkIntervalMs_;
    LedPairColor lastDisplayedColor_;

    bool greenRequested_ = false;
    bool redRequested_ = false;

    Mode currentMode_ = Mode::Blink;
    bool blinkShowingLastColor_ = true;
    unsigned long lastToggleTime_ = 0;

    bool identifying_ = false;
    bool identifyShowingGreen_ = true;
    unsigned long identifyLastToggleMs_ = 0;
    static constexpr unsigned long kIdentifyIntervalMs = 150;
};
```

- [ ] **Step 4: Write `IndependentLedPairDriver.cpp`**

Create `lib/McsEsp32/src/domain/IndependentLedPairDriver.cpp`:

```cpp
#include "IndependentLedPairDriver.h"

IndependentLedPairDriver::IndependentLedPairDriver(DigitalOutput& greenGpio, DigitalOutput& redGpio,
                                                     Clock& clock, const unsigned long blinkIntervalMs,
                                                     const LedPairColor defaultColor)
    : greenGpio_(greenGpio), redGpio_(redGpio), clock_(clock), blinkIntervalMs_(blinkIntervalMs),
      lastDisplayedColor_(defaultColor)
{
    lastToggleTime_ = clock_.nowMilliseconds();
    writeColor(lastDisplayedColor_);
}

void IndependentLedPairDriver::begin()
{
    lastToggleTime_ = clock_.nowMilliseconds();
    writeColor(currentColorToShow());
}

void IndependentLedPairDriver::setGreen(const bool active)
{
    greenRequested_ = active;
    applyState();
}

void IndependentLedPairDriver::setRed(const bool active)
{
    redRequested_ = active;
    applyState();
}

bool IndependentLedPairDriver::isGreenRequested() const
{
    return greenRequested_;
}

bool IndependentLedPairDriver::isRedRequested() const
{
    return redRequested_;
}

void IndependentLedPairDriver::setIdentifying(const bool active)
{
    if (active == identifying_)
    {
        return;
    }
    identifying_ = active;
    if (identifying_)
    {
        identifyShowingGreen_ = true;
        identifyLastToggleMs_ = clock_.nowMilliseconds();
        writeColor(LedPairColor::Green);
    }
    else
    {
        writeColor(currentColorToShow());
    }
}

void IndependentLedPairDriver::applyState()
{
    Mode nextMode;
    if (greenRequested_ && !redRequested_)
    {
        nextMode = Mode::Green;
    }
    else if (redRequested_ && !greenRequested_)
    {
        nextMode = Mode::Red;
    }
    else if (!greenRequested_ && !redRequested_)
    {
        nextMode = Mode::Blink;
    }
    else
    {
        return; // transient both-requested state: leave the GPIOs exactly as they were
    }

    if (nextMode == currentMode_)
    {
        return;
    }

    currentMode_ = nextMode;

    if (nextMode == Mode::Green)
    {
        lastDisplayedColor_ = LedPairColor::Green;
        writeColor(LedPairColor::Green);
    }
    else if (nextMode == Mode::Red)
    {
        lastDisplayedColor_ = LedPairColor::Red;
        writeColor(LedPairColor::Red);
    }
    else
    {
        blinkShowingLastColor_ = true;
        lastToggleTime_ = clock_.nowMilliseconds();
        writeColor(lastDisplayedColor_);
    }
}

void IndependentLedPairDriver::writeColor(const LedPairColor color)
{
    greenGpio_.set(color == LedPairColor::Green);
    redGpio_.set(color == LedPairColor::Red);
}

LedPairColor IndependentLedPairDriver::currentColorToShow() const
{
    if (currentMode_ == Mode::Green)
    {
        return LedPairColor::Green;
    }
    if (currentMode_ == Mode::Red)
    {
        return LedPairColor::Red;
    }

    const LedPairColor opposite =
        lastDisplayedColor_ == LedPairColor::Green ? LedPairColor::Red : LedPairColor::Green;
    return blinkShowingLastColor_ ? lastDisplayedColor_ : opposite;
}

void IndependentLedPairDriver::update()
{
    if (identifying_)
    {
        const unsigned long now = clock_.nowMilliseconds();
        if (now - identifyLastToggleMs_ >= kIdentifyIntervalMs)
        {
            identifyShowingGreen_ = !identifyShowingGreen_;
            identifyLastToggleMs_ = now;
            writeColor(identifyShowingGreen_ ? LedPairColor::Green : LedPairColor::Red);
        }
        return;
    }

    if (currentMode_ != Mode::Blink)
    {
        return;
    }

    const unsigned long now = clock_.nowMilliseconds();
    if (now - lastToggleTime_ < blinkIntervalMs_)
    {
        return;
    }

    blinkShowingLastColor_ = !blinkShowingLastColor_;
    lastToggleTime_ = now;

    const LedPairColor opposite =
        lastDisplayedColor_ == LedPairColor::Green ? LedPairColor::Red : LedPairColor::Green;
    writeColor(blinkShowingLastColor_ ? lastDisplayedColor_ : opposite);
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_independent_led_pair_driver`
Expected: PASS — all 17 cases.

- [ ] **Step 6: Run the existing LedPairDriver suite to confirm zero collateral change**

Run: `pio test -e native -f test_led_pair_driver`
Expected: PASS — unchanged (this task never touched `LedPairDriver.h`/`.cpp`).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/domain/IndependentLedPairDriver.h lib/McsEsp32/src/domain/IndependentLedPairDriver.cpp
git add test/test_independent_led_pair_driver/test_main.cpp
```

Use `/arlo-commits` to compose and create the commit — do not hand-write the message.

---

## Task 2: `IndependentLedPairOutput`

**Files:**
- Create: `lib/McsEsp32/src/adapters/IndependentLedPairOutput.h`
- Create: `lib/McsEsp32/src/adapters/IndependentLedPairOutput.cpp`
- Test: `test/test_independent_led_pair_output/test_main.cpp`

**Interfaces:**
- Consumes: `IndependentLedPairDriver` (Task 1) — `setGreen(bool)`, `setRed(bool)`, `isGreenRequested() const`, `isRedRequested() const`. `LedPairColor` (reused from `LedPairDriver.h`). `DigitalOutput` port (`lib/McsCore/src/ports/`).
- Produces: `IndependentLedPairOutput(IndependentLedPairDriver& driver, LedPairColor color)` implementing `DigitalOutput`.

- [ ] **Step 1: Write the failing tests**

Create `test/test_independent_led_pair_output/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/IndependentLedPairOutput.h"
#include "domain/IndependentLedPairDriver.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalOutput.h"

namespace
{
    constexpr unsigned long BLINK_INTERVAL_MS = 100;
}

TEST_CASE("a green-side IndependentLedPairOutput forwards set() to the driver's setGreen")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    IndependentLedPairOutput greenOutput(driver, LedPairColor::Green);

    greenOutput.set(true);

    REQUIRE(driver.isGreenRequested());
    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}

TEST_CASE("a red-side IndependentLedPairOutput forwards set() to the driver's setRed")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);
    IndependentLedPairOutput redOutput(driver, LedPairColor::Red);

    redOutput.set(true);

    REQUIRE(driver.isRedRequested());
    REQUIRE_FALSE(greenGpio.isSet());
    REQUIRE(redGpio.isSet());
}

TEST_CASE("isSet() reflects the driver's tracked request for its own color, independent of the other side")
{
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    FakeClock clock;
    IndependentLedPairDriver driver(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);
    IndependentLedPairOutput greenOutput(driver, LedPairColor::Green);
    IndependentLedPairOutput redOutput(driver, LedPairColor::Red);

    redOutput.set(true);

    REQUIRE(redOutput.isSet());
    REQUIRE_FALSE(greenOutput.isSet());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_independent_led_pair_output`
Expected: FAIL to compile — `IndependentLedPairOutput` is not declared.

- [ ] **Step 3: Write `IndependentLedPairOutput.h`**

Create `lib/McsEsp32/src/adapters/IndependentLedPairOutput.h`:

```cpp
#pragma once

#include "../domain/IndependentLedPairDriver.h"
#include "ports/DigitalOutput.h"

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

- [ ] **Step 4: Write `IndependentLedPairOutput.cpp`**

Create `lib/McsEsp32/src/adapters/IndependentLedPairOutput.cpp`:

```cpp
#include "IndependentLedPairOutput.h"

IndependentLedPairOutput::IndependentLedPairOutput(IndependentLedPairDriver& driver, const LedPairColor color)
    : driver_(driver), color_(color)
{
}

void IndependentLedPairOutput::set(const bool active)
{
    if (color_ == LedPairColor::Green)
    {
        driver_.setGreen(active);
    }
    else
    {
        driver_.setRed(active);
    }
}

bool IndependentLedPairOutput::isSet() const
{
    return color_ == LedPairColor::Green ? driver_.isGreenRequested() : driver_.isRedRequested();
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_independent_led_pair_output`
Expected: PASS — all 3 cases.

- [ ] **Step 6: Run the existing LedPairOutput suite and the full native suite**

Run: `pio test -e native -f test_led_pair_output`
Expected: PASS — unchanged.

Run: `pio test -e native`
Expected: all pre-existing suites pass unchanged, plus the two new suites added by this plan (50 suites total: 48 from before this plan + `test_independent_led_pair_driver` + `test_independent_led_pair_output`).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/adapters/IndependentLedPairOutput.h lib/McsEsp32/src/adapters/IndependentLedPairOutput.cpp
git add test/test_independent_led_pair_output/test_main.cpp
```

Use `/arlo-commits` to compose and create the commit.

---

## Self-Review Notes

- **Spec coverage:** both components from the spec's "Components" section are covered (Task 1: `IndependentLedPairDriver`; Task 2: `IndependentLedPairOutput`). The spec's full "Testing" section's case list is covered 1:1 by both tasks' test files. The spec's "Non-goals" (composition helper, real GPIO wiring, `LedPairDriver`/`LedPairOutput` changes, identify-overlay wiring, status/activity LEDs, buttons, setup trigger) are correctly absent from both tasks.
- **Placeholder scan:** no TBD/TODO; every step has complete, exact code.
- **Type consistency:** `IndependentLedPairDriver`'s constructor signature and method names are identical between Task 1's header and every call site in Task 2's test file and `IndependentLedPairOutput.cpp`.
