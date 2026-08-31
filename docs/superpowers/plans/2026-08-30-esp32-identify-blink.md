# ESP32 Identify-Blink Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a technician make a specific ESP32 turnout panel visually identify itself (all 12 LED pairs flash green/red in unison) by publishing to a new `panel/<nodeId>/identify` MQTT topic.

**Architecture:** A new, simple domain timer (`IdentifyModeTimer`, elapsed-time check, no `update()` needed) decides whether identify mode is currently active. `LedPairDriver` (already-shipped, already-tested) gains a `setIdentifying(bool)` override that takes priority over its existing per-turnout blink logic. `main.cpp` wires an MQTT subscription to the timer and calls `setIdentifying()` on all 12 stations every tick.

**Tech Stack:** C++17, PlatformIO (`native` for tests, `esp32dev` for the real build), Catch2 for native tests.

## Global Constraints

- `IDENTIFY_DURATION_MS = 10000` lives in `src/esp32/main.cpp`'s existing anonymous namespace, alongside `SETUP_TRIGGER_HOLD_MS` and the other constants.
- `kIdentifyIntervalMs = 150` is a `private static constexpr unsigned long` inside `LedPairDriver` itself (not a `main.cpp` constant) — it's an implementation detail of the flash rate, not something the composition root chooses.
- `LedPairDriver::setIdentifying(bool active)` **must be idempotent**: a no-op if `active` equals the currently-stored `identifying_` value. This is load-bearing, not a nicety — `main.cpp` calls it every single `loop()` tick with `identifyTimer.isActive()`'s current value, and if `setIdentifying(true)` reset the toggle timer on every call, the flash would never actually alternate (it would re-stamp `identifyLastToggleMs_` every tick and the elapsed-time check would never see enough time pass).
- Buttons and turnout feedback stay **fully live** during identify — no suppression. This is a pure visual overlay, unlike sub-project #2d-a's collision handling, which suppresses buttons because two panels sending commands is genuinely harmful; identify has no such hazard.
- **Accepted, not fixed:** if real turnout feedback arrives while identify is active, `applyState()` (unchanged, pre-existing method) writes directly to the GPIO, which can briefly desynchronize that one LED pair from the rest of the panel's flash for up to one `kIdentifyIntervalMs` (150ms) before the next identify tick corrects it. Do not add code to prevent this.
- The new `setIdentifying()` call in `main.cpp`'s `loop()` is its own per-tick `for` loop, kept separate from the existing `ledStation.update()` loop — `LedPairStation::update()`'s signature does not change.
- Any message on `panel/<nodeId>/identify` triggers/refreshes the timer — the payload is ignored (the lambda subscribed to this topic takes `const std::string&` but does nothing with it).

---

### Task 1: `IdentifyModeTimer`

**Files:**
- Create: `lib/McsEsp32/src/domain/IdentifyModeTimer.h`
- Create: `lib/McsEsp32/src/domain/IdentifyModeTimer.cpp`
- Test: `test/test_identify_mode_timer/test_main.cpp`

**Interfaces:**
- Consumes: `Clock::nowMilliseconds() const` (existing port, `lib/McsCore/src/ports/Clock.h`).
- Produces: `IdentifyModeTimer(Clock& clock, unsigned long durationMs)` constructor; `void trigger()`; `[[nodiscard]] bool isActive() const` — consumed by Task 5 (`main.cpp`'s MQTT subscription calls `trigger()`; `loop()` calls `isActive()` once per tick).

Independent of Tasks 2, 3, and 4 — different files, no shared code. No `update()` method is needed: `isActive()` computes the answer on demand from the current clock time, unlike `ComboSetupModeTrigger`'s edge-detection which genuinely needs a per-tick `update()` call.

- [ ] **Step 1: Write the failing tests**

Create `test/test_identify_mode_timer/test_main.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/IdentifyModeTimer.h"
#include "support/FakeClock.h"

namespace
{
    constexpr unsigned long DURATION_MS = 10000;
}

TEST_CASE("isActive is false before any trigger")
{
    FakeClock clock;
    IdentifyModeTimer timer(clock, DURATION_MS);

    REQUIRE_FALSE(timer.isActive());
}

TEST_CASE("isActive is true immediately after trigger")
{
    FakeClock clock;
    IdentifyModeTimer timer(clock, DURATION_MS);

    timer.trigger();

    REQUIRE(timer.isActive());
}

TEST_CASE("isActive becomes false once the duration has elapsed")
{
    FakeClock clock;
    IdentifyModeTimer timer(clock, DURATION_MS);

    timer.trigger();
    clock.advanceBy(DURATION_MS);

    REQUIRE_FALSE(timer.isActive());
}

TEST_CASE("isActive is still true just before the duration elapses")
{
    FakeClock clock;
    IdentifyModeTimer timer(clock, DURATION_MS);

    timer.trigger();
    clock.advanceBy(DURATION_MS - 1);

    REQUIRE(timer.isActive());
}

TEST_CASE("a second trigger before expiry extends the active window past the first trigger's own deadline")
{
    FakeClock clock;
    IdentifyModeTimer timer(clock, DURATION_MS);

    timer.trigger();
    clock.advanceBy(DURATION_MS - 1);
    timer.trigger();
    clock.advanceBy(DURATION_MS - 1);

    REQUIRE(timer.isActive());
}
```

The last test case is the one that would catch a plausible wrong implementation: if `trigger()` didn't re-stamp the internal time on a second call, total elapsed time by the final `advanceBy` would be `2 * (DURATION_MS - 1)`, which exceeds `DURATION_MS`, and a non-refreshing implementation would incorrectly report `false`.

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_identify_mode_timer`
Expected: FAIL — "IdentifyModeTimer.h: No such file or directory".

- [ ] **Step 3: Write the implementation**

Create `lib/McsEsp32/src/domain/IdentifyModeTimer.h`:

```cpp
#pragma once

#include "ports/Clock.h"

class IdentifyModeTimer
{
public:
    IdentifyModeTimer(Clock& clock, unsigned long durationMs);

    void trigger();
    [[nodiscard]] bool isActive() const;

private:
    Clock& clock_;
    unsigned long durationMs_;
    bool triggered_ = false;
    unsigned long triggeredAtMs_ = 0;
};
```

Create `lib/McsEsp32/src/domain/IdentifyModeTimer.cpp`:

```cpp
#include "IdentifyModeTimer.h"

IdentifyModeTimer::IdentifyModeTimer(Clock& clock, const unsigned long durationMs)
    : clock_(clock), durationMs_(durationMs)
{
}

void IdentifyModeTimer::trigger()
{
    triggered_ = true;
    triggeredAtMs_ = clock_.nowMilliseconds();
}

bool IdentifyModeTimer::isActive() const
{
    return triggered_ && (clock_.nowMilliseconds() - triggeredAtMs_ < durationMs_);
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_identify_mode_timer`
Expected: PASS — all 5 test cases pass.

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: all suites pass (40, up from 39 — this is a new test binary).

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/domain/IdentifyModeTimer.h lib/McsEsp32/src/domain/IdentifyModeTimer.cpp test/test_identify_mode_timer/test_main.cpp
git commit -m "feat: add IdentifyModeTimer for identify-blink triggering"
```

(Classify this commit yourself using Arlo's Commit Notation per this project's CLAUDE.md — see "Committing" note at the end of this plan.)

---

### Task 2: `PresenceTopics::identifyTopic()`

**Files:**
- Modify: `lib/McsEsp32/src/domain/PresenceTopics.h`
- Test: `test/test_presence_topics/test_main.cpp`

**Interfaces:**
- Produces: `static std::string PresenceTopics::identifyTopic(int nodeId)` — consumed by Task 5 (`main.cpp`'s new MQTT subscription).

Independent of Tasks 1, 3, and 4. Mirrors the two existing methods (`statusTopic`, `macTopic`) on this already-shipped, already-tested class exactly.

The current full content of `test/test_presence_topics/test_main.cpp` is:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "domain/PresenceTopics.h"

TEST_CASE("statusTopic builds the panel status topic for a given node id")
{
    REQUIRE(PresenceTopics::statusTopic(5) == "panel/5/status");
}

TEST_CASE("macTopic builds the panel mac topic for a given node id")
{
    REQUIRE(PresenceTopics::macTopic(5) == "panel/5/mac");
}

TEST_CASE("both topics use the node id's decimal string form for a multi-digit id")
{
    REQUIRE(PresenceTopics::statusTopic(42) == "panel/42/status");
    REQUIRE(PresenceTopics::macTopic(42) == "panel/42/mac");
}
```

Do not modify these three existing test cases — only append a new one.

- [ ] **Step 1: Write the failing test**

Append to the end of `test/test_presence_topics/test_main.cpp`:

```cpp
TEST_CASE("identifyTopic builds the panel identify topic for a given node id")
{
    REQUIRE(PresenceTopics::identifyTopic(5) == "panel/5/identify");
    REQUIRE(PresenceTopics::identifyTopic(42) == "panel/42/identify");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_presence_topics`
Expected: FAIL — no member named `identifyTopic` in `PresenceTopics`.

- [ ] **Step 3: Write the implementation**

The current full content of `lib/McsEsp32/src/domain/PresenceTopics.h` is:

```cpp
#pragma once

#include <string>

class PresenceTopics
{
public:
    static std::string statusTopic(int nodeId)
    {
        return "panel/" + std::to_string(nodeId) + "/status";
    }

    static std::string macTopic(int nodeId)
    {
        return "panel/" + std::to_string(nodeId) + "/mac";
    }
};
```

Replace it with:

```cpp
#pragma once

#include <string>

class PresenceTopics
{
public:
    static std::string statusTopic(int nodeId)
    {
        return "panel/" + std::to_string(nodeId) + "/status";
    }

    static std::string macTopic(int nodeId)
    {
        return "panel/" + std::to_string(nodeId) + "/mac";
    }

    static std::string identifyTopic(int nodeId)
    {
        return "panel/" + std::to_string(nodeId) + "/identify";
    }
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_presence_topics`
Expected: PASS — all 4 test cases pass.

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: all suites pass (unchanged count from Task 1, no new binary here).

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/domain/PresenceTopics.h test/test_presence_topics/test_main.cpp
git commit -m "feat: add PresenceTopics::identifyTopic()"
```

(Classify using ACN — see "Committing" note at the end.)

---

### Task 3: `LedPairDriver::setIdentifying()`

**Files:**
- Modify: `lib/McsEsp32/src/domain/LedPairDriver.h`
- Modify: `lib/McsEsp32/src/domain/LedPairDriver.cpp`
- Test: `test/test_led_pair_driver/test_main.cpp`

**Interfaces:**
- Produces: `void LedPairDriver::setIdentifying(bool active)` — consumed by Task 4 (`LedPairStation`'s one-line forwarding method).

Independent of Tasks 1 and 2 (different files). **This is the most delicate task in the plan**: you are modifying an existing, already-shipped, already-tested class's `update()` method. Read the current full content of both files carefully before editing — every one of the 10 existing test cases in `test/test_led_pair_driver/test_main.cpp` (listed below) must still pass unmodified after your change.

The current full content of `test/test_led_pair_driver/test_main.cpp` is:

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
    REQUIRE(greenDefaultGpio.setCallCount() == 1);

    FakeDigitalOutput redDefaultGpio;
    LedPairDriver redDefaultDriver(redDefaultGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    REQUIRE_FALSE(redDefaultGpio.isSet());
    REQUIRE(redDefaultGpio.setCallCount() == 1);
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

TEST_CASE("begin() re-writes the current color and re-stamps the blink timer, for use after the GPIO's own begin()")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    // Simulate a real ArduinoDigitalOutput::begin() happening after
    // construction, which on real hardware would clobber the GPIO level
    // and leave the driver's internal timer referring to a clock time
    // before setup() ran.
    gpio.set(false);
    clock.advanceBy(50);

    driver.begin();

    REQUIRE(gpio.isSet());

    clock.advanceBy(BLINK_INTERVAL_MS - 1);
    driver.update();
    REQUIRE(gpio.isSet());

    clock.advanceBy(1);
    driver.update();
    REQUIRE_FALSE(gpio.isSet());
}
```

**Do not touch any of the above.** Only append new test cases after the last one (`"begin() re-writes..."`).

- [ ] **Step 1: Write the failing tests**

Append to the end of `test/test_led_pair_driver/test_main.cpp`:

```cpp
TEST_CASE("setIdentifying(true) immediately shows green regardless of current mode")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setRed(true);
    REQUIRE_FALSE(gpio.isSet());

    driver.setIdentifying(true);

    REQUIRE(gpio.isSet());
}

TEST_CASE("update() toggles the identify flash at its own fixed interval, independent of the blink interval")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setIdentifying(true);
    REQUIRE(gpio.isSet());

    clock.advanceBy(149);
    driver.update();
    REQUIRE(gpio.isSet());

    clock.advanceBy(1);
    driver.update();
    REQUIRE_FALSE(gpio.isSet());

    clock.advanceBy(150);
    driver.update();
    REQUIRE(gpio.isSet());
}

TEST_CASE("setIdentifying(true) called again while already active does not reset the toggle timer")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setIdentifying(true);

    clock.advanceBy(149);
    driver.setIdentifying(true);
    driver.update();
    REQUIRE(gpio.isSet());

    clock.advanceBy(1);
    driver.update();
    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("setIdentifying(false) reverts to a steady color the normal mode would show")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);

    driver.setRed(true);
    REQUIRE_FALSE(gpio.isSet());

    driver.setIdentifying(true);
    REQUIRE(gpio.isSet());

    driver.setIdentifying(false);

    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("setIdentifying(false) reverts to blink mode's current color if that is what was active before")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    driver.setIdentifying(true);
    driver.setIdentifying(false);

    REQUIRE(gpio.isSet());

    clock.advanceBy(BLINK_INTERVAL_MS);
    driver.update();
    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("update() does not run normal blink logic while identifying")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    LedPairDriver driver(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Green);

    driver.setIdentifying(true);
    clock.advanceBy(BLINK_INTERVAL_MS);
    driver.update();

    // BLINK_INTERVAL_MS (100) has elapsed, which would have flipped the
    // GPIO under the old blink logic — but the identify interval (150)
    // has not, so the short-circuit must leave it exactly as identify
    // set it (still green), proving the old blink path did not also run.
    REQUIRE(gpio.isSet());
}
```

Notes on these test cases:
- The second test's exact toggle sequence (green at t=0, still green at t=149, flips to red at t=150, flips back to green at t=300) exercises the fixed 150ms identify interval directly, independent of the `BLINK_INTERVAL_MS = 100` constant this test file's other cases use — proving identify has its own timing, not reusing `blinkIntervalMs_`.
- The third test is the load-bearing idempotency check called out in Global Constraints: if `setIdentifying(true)` reset `identifyLastToggleMs_` on the second call, the `update()` at total elapsed time 149 would not yet be due to toggle (correct either way) — the real proof is the *next* `update()` one tick later, at total elapsed 150, which only flips on schedule if the second `setIdentifying(true)` call didn't restart the clock.
- The fourth test starts from a steady-green request, so post-identify the driver should show the same steady green it would have shown had identify never happened.
- The fifth test starts from blink mode's initial state (green, matching the constructor's `defaultColor`), turns identify on then immediately off, and confirms blink mode is still in control afterward (the LED changes on the next elapsed blink interval).
- The sixth test proves `update()`'s new short-circuit genuinely bypasses the old blink-timing code path while identifying, rather than running both.

- [ ] **Step 2: Run tests to verify the new ones fail (and the old ones still pass)**

Run: `pio test -e native -f test_led_pair_driver`
Expected: the 10 pre-existing cases PASS; the 6 new cases FAIL (`setIdentifying` is not a member of `LedPairDriver`).

- [ ] **Step 3: Add `setIdentifying()` and its private state to the header**

The current full content of `lib/McsEsp32/src/domain/LedPairDriver.h` is:

```cpp
#pragma once

#include "ports/Clock.h"
#include "ports/DigitalOutput.h"

enum class LedPairColor
{
    Green,
    Red
};

// Drives a shared-GPIO red/green LED pair: one GPIO level encodes both
// colors, so there is no true "off". `update()` must be called on every
// loop iteration for blinking to work. Exactly two `LedPairOutput`s (one
// green, one red) are expected to share a single `LedPairDriver` instance.
// Green corresponds to the GPIO driven HIGH.
class LedPairDriver
{
public:
    LedPairDriver(DigitalOutput& gpio, Clock& clock, unsigned long blinkIntervalMs,
                  LedPairColor defaultColor);

    void begin();

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
    [[nodiscard]] LedPairColor currentColorToShow() const;

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

Replace it with:

```cpp
#pragma once

#include "ports/Clock.h"
#include "ports/DigitalOutput.h"

enum class LedPairColor
{
    Green,
    Red
};

// Drives a shared-GPIO red/green LED pair: one GPIO level encodes both
// colors, so there is no true "off". `update()` must be called on every
// loop iteration for blinking to work. Exactly two `LedPairOutput`s (one
// green, one red) are expected to share a single `LedPairDriver` instance.
// Green corresponds to the GPIO driven HIGH.
class LedPairDriver
{
public:
    LedPairDriver(DigitalOutput& gpio, Clock& clock, unsigned long blinkIntervalMs,
                  LedPairColor defaultColor);

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

    DigitalOutput& gpio_;
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

- [ ] **Step 4: Add `setIdentifying()`'s implementation**

In `lib/McsEsp32/src/domain/LedPairDriver.cpp`, find:

```cpp
bool LedPairDriver::isRedRequested() const
{
    return redRequested_;
}

void LedPairDriver::applyState()
```

Replace it with:

```cpp
bool LedPairDriver::isRedRequested() const
{
    return redRequested_;
}

void LedPairDriver::setIdentifying(const bool active)
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

void LedPairDriver::applyState()
```

- [ ] **Step 5: Add the short-circuit to `update()`**

In the same file, find:

```cpp
void LedPairDriver::update()
{
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

Replace it with:

```cpp
void LedPairDriver::update()
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

- [ ] **Step 6: Run tests to verify everything passes**

Run: `pio test -e native -f test_led_pair_driver`
Expected: PASS — all 16 test cases pass (10 pre-existing + 6 new).

Also run: `pio test -e native`
Expected: full suite passes, no other suite references this file's internals.

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/domain/LedPairDriver.h lib/McsEsp32/src/domain/LedPairDriver.cpp test/test_led_pair_driver/test_main.cpp
git commit -m "feat: add LedPairDriver::setIdentifying() for the identify-blink override"
```

(Classify using ACN — see "Committing" note at the end.)

---

### Task 4: `LedPairStation::setIdentifying()`

**Files:**
- Modify: `lib/McsEsp32/src/adapters/LedPairStation.h`
- Modify: `lib/McsEsp32/src/adapters/LedPairStation.cpp`

**Interfaces:**
- Consumes: `LedPairDriver::setIdentifying(bool)` (Task 3).
- Produces: `void LedPairStation::setIdentifying(bool active)` — consumed by Task 5 (`main.cpp`'s new per-tick loop).

Depends on Task 3. One-line mechanical forwarding, matching this class's existing `begin()`/`update()` methods, which each forward one call to `driver_`. `#ifdef ARDUINO`-guarded like the rest of the class — no native test, matching this class's existing convention (it has never had one).

- [ ] **Step 1: Add the header declaration**

The current full content of `lib/McsEsp32/src/adapters/LedPairStation.h` is:

```cpp
#pragma once

#include "adapters/ArduinoDigitalOutput.h"
#include "ports/Clock.h"
#include "ports/DigitalOutput.h"
#include "../domain/LedPairDriver.h"
#include "LedPairOutput.h"

struct LedPairConfig
{
    int gpioPin;
};

class LedPairStation
{
public:
    LedPairStation(const LedPairConfig& config, Clock& clock, unsigned long blinkIntervalMs,
                   LedPairColor defaultColor);

    void begin();
    void update();

    DigitalOutput& green();
    DigitalOutput& red();

private:
    ArduinoDigitalOutput gpio_;
    LedPairDriver driver_;
    LedPairOutput green_;
    LedPairOutput red_;
};
```

Replace the `void begin(); void update();` line with:

```cpp
    void begin();
    void update();
    void setIdentifying(bool active);
```

- [ ] **Step 2: Add the implementation**

The current full content of `lib/McsEsp32/src/adapters/LedPairStation.cpp` is:

```cpp
#ifdef ARDUINO

#include "LedPairStation.h"

LedPairStation::LedPairStation(const LedPairConfig& config, Clock& clock,
                                const unsigned long blinkIntervalMs, const LedPairColor defaultColor)
    : gpio_(config.gpioPin, false)
    , driver_(gpio_, clock, blinkIntervalMs, defaultColor)
    , green_(driver_, LedPairColor::Green)
    , red_(driver_, LedPairColor::Red)
{
}

void LedPairStation::begin()
{
    gpio_.begin();
    driver_.begin();
}

void LedPairStation::update()
{
    driver_.update();
}

DigitalOutput& LedPairStation::green()
{
    return green_;
}

DigitalOutput& LedPairStation::red()
{
    return red_;
}

#endif
```

Replace:

```cpp
void LedPairStation::update()
{
    driver_.update();
}
```

with:

```cpp
void LedPairStation::update()
{
    driver_.update();
}

void LedPairStation::setIdentifying(const bool active)
{
    driver_.setIdentifying(active);
}
```

- [ ] **Step 3: Run the esp32dev build**

Run: `pio run -e esp32dev`
Expected: SUCCESS. (This class is guarded out of `native` entirely — `McsEsp32` is listed in `esp32dev`'s `lib_deps`, so PlatformIO compiles this `.cpp` regardless of whether `main.cpp` calls the new method yet; a signature error here would still fail the build on its own.)

Also run: `pio test -e native`
Expected: unaffected (this file never builds under `native`).

- [ ] **Step 4: Commit**

```bash
git add lib/McsEsp32/src/adapters/LedPairStation.h lib/McsEsp32/src/adapters/LedPairStation.cpp
git commit -m "feat: forward setIdentifying() through LedPairStation"
```

(Classify using ACN — see "Committing" note at the end.)

---

### Task 5: Wire identify-blink into `src/esp32/main.cpp`

**Files:**
- Modify: `src/esp32/main.cpp`

**Interfaces:**
- Consumes: `IdentifyModeTimer(Clock&, unsigned long)` with `void trigger()` and `bool isActive() const` (Task 1); `PresenceTopics::identifyTopic(int)` returning `std::string` (Task 2); `LedPairStation::setIdentifying(bool)` (Task 4, which itself depends on Task 3).
- Produces: nothing consumed by a later task — this is the final task.

This is composition-root wiring with zero test coverage of its own (`src/` is excluded from `native`, `test_build_src = false`) — the same situation as every prior `main.cpp`-wiring task in this project. Verification is `pio run -e esp32dev` build success plus the numbered manual-read-through checklist at Step 6. Depends on Tasks 1, 2, and 4 (which itself depends on Task 3) all being complete.

Read the current `src/esp32/main.cpp` yourself before editing to confirm it matches every "replace this" block below exactly — this file was last touched by sub-project #2d-a and nothing since, but verify rather than assume.

- [ ] **Step 1: Add the new include**

Replace the existing include block:

```cpp
#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/ComboSetupModeTrigger.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspUartPort.h"
#include "adapters/GatedDigitalInput.h"
#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
#include "adapters/LedPairStation.h"
#include "adapters/MatrixDigitalInput.h"
#include "adapters/MqttLink.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/WiFiLink.h"
#include "application/CommissioningSession.h"
#include "application/MqttPresenceAnnouncer.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/LedPairDriver.h"
#include "domain/MatrixScanner.h"
#include "domain/NodeConfig.h"
#include "domain/NodeIdentityGuard.h"
#include "domain/PresenceTopics.h"
#include "domain/SetupApName.h"
#include "ports/TurnoutCommandPort.h"
```

with:

```cpp
#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/ComboSetupModeTrigger.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspUartPort.h"
#include "adapters/GatedDigitalInput.h"
#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
#include "adapters/LedPairStation.h"
#include "adapters/MatrixDigitalInput.h"
#include "adapters/MqttLink.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "adapters/WiFiLink.h"
#include "application/CommissioningSession.h"
#include "application/MqttPresenceAnnouncer.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/IdentifyModeTimer.h"
#include "domain/LedPairDriver.h"
#include "domain/MatrixScanner.h"
#include "domain/NodeConfig.h"
#include "domain/NodeIdentityGuard.h"
#include "domain/PresenceTopics.h"
#include "domain/SetupApName.h"
#include "ports/TurnoutCommandPort.h"
```

- [ ] **Step 2: Add the `IDENTIFY_DURATION_MS` constant**

Replace:

```cpp
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr const char* WIRELESS_SETUP_AP_PASSPHRASE = "maltbee-setup";
}
```

with:

```cpp
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr const char* WIRELESS_SETUP_AP_PASSPHRASE = "maltbee-setup";
    constexpr unsigned long IDENTIFY_DURATION_MS = 10000;
}
```

- [ ] **Step 3: Add the `identifyTimer` global**

Replace:

```cpp
NodeIdentityGuard identityGuard(ownMac.lastFourHexDigits());
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, runningConfig.nodeId, ownMac.lastFourHexDigits());

JmriTurnoutCommandAdapter turnoutCommandPort(mqttLink, runningConfig.channelJmriNames);
```

with:

```cpp
NodeIdentityGuard identityGuard(ownMac.lastFourHexDigits());
MqttPresenceAnnouncer presenceAnnouncer(mqttLink, runningConfig.nodeId, ownMac.lastFourHexDigits());
IdentifyModeTimer identifyTimer(systemClock, IDENTIFY_DURATION_MS);

JmriTurnoutCommandAdapter turnoutCommandPort(mqttLink, runningConfig.channelJmriNames);
```

- [ ] **Step 4: Add the identify-topic subscription in `setup()`**

Replace:

```cpp
    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
        mqttLink.subscribe(PresenceTopics::macTopic(runningConfig.nodeId),
                            [](const std::string& payload) { identityGuard.onMacObserved(payload); });
    }
```

with:

```cpp
    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
        mqttLink.subscribe(PresenceTopics::macTopic(runningConfig.nodeId),
                            [](const std::string& payload) { identityGuard.onMacObserved(payload); });
        mqttLink.subscribe(PresenceTopics::identifyTopic(runningConfig.nodeId),
                            [](const std::string&) { identifyTimer.trigger(); });
    }
```

The lambda's parameter is unnamed (`const std::string&`) since the payload is deliberately ignored — any message triggers identify.

- [ ] **Step 5: Add the identify loop in `loop()`**

Replace:

```cpp
    for (auto& ledStation : ledStations)
    {
        ledStation.update();
    }

    for (auto& station : stations)
    {
        station.update();
    }
}
```

with:

```cpp
    const bool identifying = identifyTimer.isActive();
    for (auto& ledStation : ledStations)
    {
        ledStation.setIdentifying(identifying);
    }

    for (auto& ledStation : ledStations)
    {
        ledStation.update();
    }

    for (auto& station : stations)
    {
        station.update();
    }
}
```

This is a new, separate loop placed immediately before the existing `ledStation.update()` loop — `LedPairStation::update()`'s own signature is unchanged, and `setIdentifying()` must be called before `update()` each tick so a fresh identify activation takes effect the same tick it starts.

- [ ] **Step 6: Run the esp32dev build**

Run: `pio run -e esp32dev`
Expected: SUCCESS. Note the RAM/Flash usage reported — expect a very small increase over the pre-task baseline (sub-project #2d-a's merged state: RAM 16.8%/Flash 81.1%), since one small new class is now linked in.

- [ ] **Step 7: Run the megaatmega2560 build (regression check)**

Run: `pio run -e megaatmega2560`
Expected: SUCCESS, usage unchanged — `src/esp32/main.cpp` is excluded from this environment's `build_src_filter` entirely.

- [ ] **Step 8: Run the full native suite (regression check)**

Run: `pio test -e native`
Expected: all suites pass, unchanged count from Task 4 (this file is never part of the `native` build).

- [ ] **Step 9: Manually re-read `setup()`/`loop()` against this task's target code**

Confirm, citing actual file line numbers in your report:

(a) `identifyTimer` is declared once, as a global, after `presenceAnnouncer` and before `turnoutCommandPort`.

(b) The identify-topic subscription appears exactly once, inside `setup()`'s `if (configValid)` block, after the mac-topic subscription — not before it, and not duplicated in `loop()`.

(c) `identifying` is computed exactly once per `loop()` tick from `identifyTimer.isActive()`, and the same local is used for all 12 `setIdentifying()` calls in that tick (not recomputed per station).

(d) The new `setIdentifying()` loop runs over all 12 `ledStations`, with no index skipped or duplicated, and appears BEFORE the existing `ledStation.update()` loop (not after, and not interleaved with it).

(e) No other loop or condition in `loop()` was altered — the collision-suppression logic, the feedback/clear-indicator gate, and the `presenceAnnouncer.update()` call from sub-project #2d-a are all untouched by this task.

- [ ] **Step 10: Commit**

```bash
git add src/esp32/main.cpp
git commit -m "feat: wire identify-blink MQTT trigger into main.cpp"
```

(Classify using ACN — see "Committing" note at the end.)

---

## Committing

This project's `CLAUDE.md` requires every commit to be classified with Arlo's Commit Notation (`<risk symbol> <intention letter> <description>`) via the `/arlo-commits` skill's rules, not the placeholder `feat:`-style messages shown in this plan's steps above. When executing this plan (directly or via a dispatched implementer), replace each placeholder commit message with a real ACN-classified one:
- Any `F`/`B` commit touching more than 8 lines (including test changes) is automatically capped at `!` (Risky), never `^`/`.`, regardless of test coverage — check the actual diff size before choosing a symbol.
- Task 4's `LedPairStation` commit has no native test coverage at all (by design — it's `#ifdef ARDUINO`-guarded), which independently supports `!` there even though the diff is tiny.
- Task 5's `main.cpp` commit has no native test coverage at all (by design — `test_build_src = false` excludes `src/`), which independently supports `!` there too.
- Format the summary line with a literal space between the risk symbol and the intention letter (`! F description`, not `!F description`).
