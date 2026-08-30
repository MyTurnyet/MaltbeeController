# ESP32 Shared-GPIO LED-Pair Driver (Sub-project #4) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the software that drives one shared GPIO carrying a red LED
and a green LED wired in opposite directions (per `docs/led-wiring.md`),
translating the existing `TurnoutIndicator`'s `on()`/`off()` call pattern
into steady green, steady red, or a non-blocking blink between the
last-displayed color and its opposite when both sides are requested off —
without any change to `Indicator`, `TurnoutIndicator`, or `TurnoutControl`.

**Architecture:** Two new classes in `lib/McsEsp32/` (not `lib/McsCore/` —
this is ESP32-only; the Mega has two independent GPIOs and never needs
this). `LedPairDriver` (domain) owns the shared `DigitalOutput&`, tracks two
independent boolean requests, and derives steady-green/steady-red/blink from
them, handling blink timing via the existing `Clock` port. `LedPairOutput`
(adapter) implements the existing `DigitalOutput` port for one logical side
(green or red), forwarding to the driver — this is what lets sub-project #7
build two ordinary `Indicator`s around one `LedPairDriver` and hand them to
an unmodified `TurnoutIndicator`, exactly like the Mega does with two real
GPIOs today.

**Tech Stack:** C++17, PlatformIO, Catch2 (native tests). No hardware, no
Arduino-guarded code, and no `esp32dev`/`src/esp32/main.cpp` changes in this
plan — the actual blink interval value, default color per turnout, and real
GPIO construction are composition-root work (sub-project #7), not this
plan's.

## Global Constraints

- Domain and port headers must compile under `native` with no `Arduino.h`
  (this plan touches no Arduino-guarded files at all).
- `lib/McsEsp32` targets `native` and `esp32dev` only, both with full
  libstdc++.
- No mocking framework. `FakeDigitalOutput`/`FakeClock` (`test/support/`)
  already exist and are reused unchanged by both tasks.
- `LedPairDriver` takes its blink interval as a constructor parameter
  (`unsigned long blinkIntervalMs`), matching `PulsingLocoNetTransport`'s
  existing precedent (`lib/McsLoconet/src/adapters/PulsingLocoNetTransport.h`)
  of taking a timing value as a constructor argument rather than hardcoding
  it — the real millisecond value is sub-project #7's decision, not this
  plan's.
- `Indicator::isOn()`/`DigitalOutput::isSet()` reflect the last *logical*
  request made, not a live re-read of the physical GPIO — this already
  matches `ArduinoDigitalOutput`'s existing convention (it tracks its own
  `active_` field rather than reading the pin back). `LedPairOutput::isSet()`
  must follow the same convention: it stays true for whichever color was
  last requested even while blink is visually alternating the real GPIO.
- Requesting both green and red simultaneously true is a transient,
  never-a-final-state condition (it only happens mid-way through
  `TurnoutIndicator::display()`'s two sequential calls) — the driver must
  leave the GPIO exactly as it was when this occurs, not write anything.
- A redundant request that doesn't change the derived mode (e.g. requesting
  green again while already steady-green, or requesting off again while
  already blinking) must not reset the blink timer or rewrite the GPIO.
- This plan does **not** touch `src/esp32/main.cpp`, `Indicator`,
  `TurnoutIndicator`, `TurnoutControl`, or `TurnoutPosition` — all of that
  is sub-project #7's composition-root work, or already-unmodified reuse.
  `TurnoutState`/`UNKNOWN` is purely a `LedPairDriver`-internal concept; it
  is not added to the shared domain `TurnoutPosition` enum.
- Commit messages use this project's Arlo's Commit Notation (ACN) —
  `<risk symbol> <intention letter> <description>` — per `CLAUDE.md`.

---

### Task 1: `LedPairDriver`

**Files:**
- Create: `lib/McsEsp32/src/domain/LedPairDriver.h`
- Create: `lib/McsEsp32/src/domain/LedPairDriver.cpp`
- Test: `test/test_led_pair_driver/test_main.cpp`

**Interfaces:**
- Consumes: `DigitalOutput`/`Clock` (existing, `lib/McsCore/src/ports/`,
  rooted includes `"ports/DigitalOutput.h"`/`"ports/Clock.h"`, matching
  `McsLoconet`'s existing convention for the same kind of cross-library
  dependency); `FakeDigitalOutput`/`FakeClock` (existing, `test/support/`).
- Produces: `enum class LedPairColor { Green, Red };` and `class
  LedPairDriver { LedPairDriver(DigitalOutput& gpio, Clock& clock,
  unsigned long blinkIntervalMs, LedPairColor defaultColor); void
  setGreen(bool active); void setRed(bool active); bool isGreenRequested()
  const; bool isRedRequested() const; void update(); }`. Task 2 consumes
  both `LedPairColor` and `LedPairDriver` by reference.

- [ ] **Step 1: Write the failing tests**

Create `test/test_led_pair_driver/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/LedPairDriver.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalOutput.h"

namespace
{
    constexpr unsigned long BLINK_INTERVAL_MS = 100;
}

TEST_CASE("requesting green writes the GPIO HIGH immediately")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setGreen(true);

    REQUIRE(gpio.isSet());
}

TEST_CASE("requesting red writes the GPIO LOW immediately")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    driver.setRed(true);

    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("before anything has been requested, the GPIO already shows the configured default color")
{
    FakeDigitalOutput greenDefaultGpio;
    FakeClock clock;
    LedPairDriver greenDefaultDriver(greenDefaultGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    REQUIRE(greenDefaultGpio.isSet());

    FakeDigitalOutput redDefaultGpio;
    LedPairDriver redDefaultDriver(redDefaultGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    REQUIRE_FALSE(redDefaultGpio.isSet());
}

TEST_CASE("requesting both off enters blink and immediately shows the last-displayed color, from green")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setGreen(true);
    REQUIRE(gpio.isSet());

    driver.setGreen(false);

    REQUIRE(gpio.isSet());
}

TEST_CASE("requesting both off enters blink and immediately shows the last-displayed color, from red")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    driver.setRed(true);
    REQUIRE_FALSE(gpio.isSet());

    driver.setRed(false);

    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("update() before the blink interval elapses does nothing")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    clock.advanceBy(BLINK_INTERVAL_MS - 1);
    driver.update();

    REQUIRE(gpio.isSet());
}

TEST_CASE("update() after the blink interval elapses flips to the opposite color and resets the timer")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    clock.advanceBy(BLINK_INTERVAL_MS);
    driver.update();
    REQUIRE_FALSE(gpio.isSet());

    clock.advanceBy(BLINK_INTERVAL_MS);
    driver.update();
    REQUIRE(gpio.isSet());
}

TEST_CASE("update() never touches the GPIO while a steady color is requested")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setGreen(true);
    clock.advanceBy(BLINK_INTERVAL_MS * 10);
    driver.update();

    REQUIRE(gpio.isSet());
}

TEST_CASE("a redundant call that does not change the derived mode does not reset the blink timer")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    clock.advanceBy(BLINK_INTERVAL_MS - 1);
    driver.setGreen(false);
    driver.setRed(false);

    clock.advanceBy(1);
    driver.update();

    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("requesting both colors simultaneously leaves the GPIO exactly as it was")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setGreen(true);
    REQUIRE(gpio.isSet());

    driver.setRed(true);
    REQUIRE(gpio.isSet());

    driver.setGreen(false);

    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("isGreenRequested and isRedRequested reflect the last request for each side independently")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    driver.setRed(true);

    REQUIRE(driver.isRedRequested());
    REQUIRE_FALSE(driver.isGreenRequested());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_led_pair_driver`
Expected: FAIL to compile — `domain/LedPairDriver.h` does not exist yet.

- [ ] **Step 3: Write `lib/McsEsp32/src/domain/LedPairDriver.h`**

```cpp
#pragma once

#include "ports/Clock.h"
#include "ports/DigitalOutput.h"

enum class LedPairColor
{
    Green,
    Red
};

class LedPairDriver
{
public:
    LedPairDriver(DigitalOutput& gpio, Clock& clock, unsigned long blinkIntervalMs,
                  LedPairColor defaultColor);

    void setGreen(bool active);
    void setRed(bool active);

    [[nodiscard]] bool isGreenRequested() const;
    [[nodiscard]] bool isRedRequested() const;

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

    DigitalOutput& gpio_;
    Clock& clock_;
    unsigned long blinkIntervalMs_;
    LedPairColor lastDisplayedColor_;

    bool greenRequested_ = false;
    bool redRequested_ = false;

    Mode currentMode_ = Mode::Blink;
    bool blinkShowingLastColor_ = true;
    unsigned long lastToggleTime_ = 0;
};
```

- [ ] **Step 4: Write `lib/McsEsp32/src/domain/LedPairDriver.cpp`**

```cpp
#include "LedPairDriver.h"

LedPairDriver::LedPairDriver(DigitalOutput& gpio, Clock& clock, const unsigned long blinkIntervalMs,
                              const LedPairColor defaultColor)
    : gpio_(gpio), clock_(clock), blinkIntervalMs_(blinkIntervalMs), lastDisplayedColor_(defaultColor)
{
    lastToggleTime_ = clock_.nowMilliseconds();
    writeColor(lastDisplayedColor_);
}

void LedPairDriver::setGreen(const bool active)
{
    greenRequested_ = active;
    applyState();
}

void LedPairDriver::setRed(const bool active)
{
    redRequested_ = active;
    applyState();
}

bool LedPairDriver::isGreenRequested() const
{
    return greenRequested_;
}

bool LedPairDriver::isRedRequested() const
{
    return redRequested_;
}

void LedPairDriver::applyState()
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
        return; // transient both-requested state: leave the GPIO exactly as it was
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

void LedPairDriver::writeColor(const LedPairColor color)
{
    gpio_.set(color == LedPairColor::Green);
}

void LedPairDriver::update()
{
    if (currentMode_ != Mode::Blink)
    {
        return;
    }

    if (clock_.nowMilliseconds() - lastToggleTime_ < blinkIntervalMs_)
    {
        return;
    }

    blinkShowingLastColor_ = !blinkShowingLastColor_;
    lastToggleTime_ = clock_.nowMilliseconds();

    const LedPairColor opposite =
        lastDisplayedColor_ == LedPairColor::Green ? LedPairColor::Red : LedPairColor::Green;
    writeColor(blinkShowingLastColor_ ? lastDisplayedColor_ : opposite);
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_led_pair_driver`
Expected: PASS, all 10 test cases green.

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every existing suite plus the new one green (26 suites
total — 25 existing + 1 new).

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/domain/LedPairDriver.h lib/McsEsp32/src/domain/LedPairDriver.cpp test/test_led_pair_driver/test_main.cpp
git commit -m "! F Add LedPairDriver for the ESP32 panel's shared-GPIO LED pairs"
```

---

### Task 2: `LedPairOutput`

**Files:**
- Create: `lib/McsEsp32/src/adapters/LedPairOutput.h`
- Create: `lib/McsEsp32/src/adapters/LedPairOutput.cpp`
- Test: `test/test_led_pair_output/test_main.cpp`

**Interfaces:**
- Consumes: `LedPairDriver`/`LedPairColor` (Task 1, by reference/value);
  `DigitalOutput` (existing, `lib/McsCore/src/ports/DigitalOutput.h`,
  rooted include).
- Produces: `class LedPairOutput final : public DigitalOutput {
  LedPairOutput(LedPairDriver& driver, LedPairColor color); void
  set(bool active) override; bool isSet() const override; }`. No later task
  in this plan consumes this — sub-project #7 constructs 24 of these (2 per
  turnout, sharing 12 `LedPairDriver`s) directly.

- [ ] **Step 1: Write the failing tests**

Create `test/test_led_pair_output/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/LedPairOutput.h"
#include "domain/LedPairDriver.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalOutput.h"

namespace
{
    constexpr unsigned long BLINK_INTERVAL_MS = 100;
}

TEST_CASE("a green-side LedPairOutput forwards set() to the driver's setGreen")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    LedPairOutput greenOutput(driver, LedPairColor::Green);

    greenOutput.set(true);

    REQUIRE(driver.isGreenRequested());
    REQUIRE(gpio.isSet());
}

TEST_CASE("a red-side LedPairOutput forwards set() to the driver's setRed")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);
    LedPairOutput redOutput(driver, LedPairColor::Red);

    redOutput.set(true);

    REQUIRE(driver.isRedRequested());
    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("isSet() reflects the driver's tracked request for its own color, independent of the other side")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);
    LedPairOutput greenOutput(driver, LedPairColor::Green);
    LedPairOutput redOutput(driver, LedPairColor::Red);

    redOutput.set(true);

    REQUIRE(redOutput.isSet());
    REQUIRE_FALSE(greenOutput.isSet());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_led_pair_output`
Expected: FAIL to compile — `adapters/LedPairOutput.h` does not exist yet.

- [ ] **Step 3: Write `lib/McsEsp32/src/adapters/LedPairOutput.h`**

```cpp
#pragma once

#include "../domain/LedPairDriver.h"
#include "ports/DigitalOutput.h"

class LedPairOutput final : public DigitalOutput
{
public:
    LedPairOutput(LedPairDriver& driver, LedPairColor color);

    void set(bool active) override;
    [[nodiscard]] bool isSet() const override;

private:
    LedPairDriver& driver_;
    LedPairColor color_;
};
```

- [ ] **Step 4: Write `lib/McsEsp32/src/adapters/LedPairOutput.cpp`**

```cpp
#include "LedPairOutput.h"

LedPairOutput::LedPairOutput(LedPairDriver& driver, const LedPairColor color)
    : driver_(driver), color_(color)
{
}

void LedPairOutput::set(const bool active)
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

bool LedPairOutput::isSet() const
{
    return color_ == LedPairColor::Green ? driver_.isGreenRequested() : driver_.isRedRequested();
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_led_pair_output`
Expected: PASS, all 3 test cases green.

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS, every suite green — 27 suites total (25 existing + 2 new
from Tasks 1-2).

- [ ] **Step 7: Build for the ESP32 and Mega targets to confirm both are unaffected**

Run: `pio run -e esp32dev`
Expected: SUCCESS — confirms both new files compile cleanly under the real
ESP32 toolchain too (via `McsEsp32`'s existing `lib_deps` entry in
`platformio.ini`), even though nothing here is `#ifdef ARDUINO`-guarded
(none of it touches Arduino APIs directly).

Run: `pio run -e megaatmega2560`
Expected: SUCCESS — `McsEsp32` is `lib_ignore`d there, so this plan's files
are never compiled for that target.

- [ ] **Step 8: Commit**

```bash
git add lib/McsEsp32/src/adapters/LedPairOutput.h lib/McsEsp32/src/adapters/LedPairOutput.cpp test/test_led_pair_output/test_main.cpp
git commit -m "! F Add LedPairOutput adapter for the ESP32 shared-GPIO LED pair"
```

---

## Definition of Done for this Plan

- [ ] `pio test -e native` passes, including the 2 new suites
      (`test_led_pair_driver`, `test_led_pair_output`) — 27 suites total
      (25 existing + 2 new).
- [ ] `pio run -e esp32dev` builds cleanly with both new files compiled in.
- [ ] `pio run -e megaatmega2560` still builds cleanly (unaffected —
      `McsEsp32` is `lib_ignore`d there — verify rather than assume).
- [ ] Two commits on `main` (or a feature branch), each `! F` — one per
      task.
- [ ] Nothing in this plan touches `src/esp32/main.cpp`, `Indicator`,
      `TurnoutIndicator`, `TurnoutControl`, or `TurnoutPosition` — the
      actual blink interval value, default color per turnout, and real
      GPIO construction/wiring for all 12 turnouts are deferred to
      sub-project #7, per
      `docs/superpowers/specs/2026-08-29-esp32-led-pair-driver-design.md`'s
      Non-goals.
