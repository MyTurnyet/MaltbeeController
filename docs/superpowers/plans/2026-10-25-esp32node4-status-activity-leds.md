# ESP32 Node4 Status/Activity LEDs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the node4 board's two new on-board LEDs — a network status LED (GPIO 18) and an activity LED (GPIO 17) — closing the last open item in the PCB design doc's firmware checklist.

**Architecture:** Two new, fully native-testable library classes (`StatusLedDriver` in `domain/`, `ActivityTrackingTurnoutCommandPort` in `adapters/`) plus reuse of the existing `IdentifyModeTimer`, wired into `src/esp32node4/main.cpp` only. Unlike recent node4 sub-projects, this one has real new TDD'd library code, not just composition-root wiring.

**Tech Stack:** C++17, PlatformIO (`native` + `esp32node4`/`esp32dev`/`megaatmega2560` environments), Catch2.

## Global Constraints

- No panel (`src/esp32/main.cpp`) or Mega (`src/mega/main.cpp`) changes — this hardware is node4-specific per the PCB doc.
- No change to `LedPairDriver`, `IndependentLedPairDriver`, `IdentifyModeTimer`, `ToggleTurnoutStation`, `ToggleTurnoutControl`, or `Loco2MqttTurnoutCommandAdapter4` — all reused unmodified.
- Network LED state: collision → `FastBlink` (150ms); no WiFi → `Off`; WiFi but no broker → `SlowBlink` (500ms); both connected → `On`.
- Activity LED: 150ms momentary flash on every Loco2MQTT command sent or feedback received, via `IdentifyModeTimer`'s existing trigger()/isActive() mechanism — no new timer class.
- `BootMode::WirelessSetup`: both new LEDs stay off — no new branch, this falls out naturally since WiFi/MQTT are never started in that mode.
- `activeLow=false` for both new `ArduinoDigitalOutput`s, matching the existing LED-GPIO convention (`IndependentLedPairStation.cpp`'s `redGpio_(config.redGpioPin, false)`).
- The activity-LED GPIO read must be the **very last statement in `loop()`**, after the `station.update()` loop — a same-tick send must flash the same tick, not one tick later (same ordering principle as sub-project #9's identify wiring).
- Both now-standard staleness sweeps run as required steps in Task 4, the general one now widened to include `README.md` per the lesson sub-project #9's final review surfaced.

---

### Task 1: `StatusLedDriver` domain class

**Files:**
- Create: `lib/McsEsp32/src/domain/StatusLedDriver.h`
- Create: `lib/McsEsp32/src/domain/StatusLedDriver.cpp`
- Create: `test/test_status_led_driver/test_main.cpp`

**Interfaces:**
- Consumes: `Clock::nowMilliseconds()` (`lib/McsCore/src/ports/Clock.h`), `DigitalOutput::set(bool)`/`isSet()` (`lib/McsCore/src/ports/DigitalOutput.h`) — both pre-existing ports, no changes.
- Produces: `StatusLedDriver(DigitalOutput& gpio, Clock& clock, unsigned long slowBlinkIntervalMs, unsigned long fastBlinkIntervalMs)`, `StatusLedDriver::State` enum (`Off`/`On`/`SlowBlink`/`FastBlink`), `void begin()`, `void setState(State)`, `void update()` — all consumed by Task 3.

- [ ] **Step 1: Write the failing tests**

Create `test/test_status_led_driver/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/StatusLedDriver.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalOutput.h"

namespace
{
    constexpr unsigned long SLOW_INTERVAL_MS = 500;
    constexpr unsigned long FAST_INTERVAL_MS = 150;
}

TEST_CASE("begins off, before setState or begin() is called")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    StatusLedDriver driver(gpio, clock, SLOW_INTERVAL_MS, FAST_INTERVAL_MS);

    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("On state lights the GPIO steady")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    StatusLedDriver driver(gpio, clock, SLOW_INTERVAL_MS, FAST_INTERVAL_MS);

    driver.setState(StatusLedDriver::State::On);

    REQUIRE(gpio.isSet());
}

TEST_CASE("Off state (even after On) keeps the GPIO steady low")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    StatusLedDriver driver(gpio, clock, SLOW_INTERVAL_MS, FAST_INTERVAL_MS);

    driver.setState(StatusLedDriver::State::On);
    driver.setState(StatusLedDriver::State::Off);

    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("SlowBlink toggles at the slow interval")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    StatusLedDriver driver(gpio, clock, SLOW_INTERVAL_MS, FAST_INTERVAL_MS);

    driver.setState(StatusLedDriver::State::SlowBlink);
    REQUIRE(gpio.isSet());

    clock.advanceBy(SLOW_INTERVAL_MS - 1);
    driver.update();
    REQUIRE(gpio.isSet());

    clock.advanceBy(1);
    driver.update();
    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("FastBlink toggles at the fast interval, distinct from the slow rate")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    StatusLedDriver driver(gpio, clock, SLOW_INTERVAL_MS, FAST_INTERVAL_MS);

    driver.setState(StatusLedDriver::State::FastBlink);
    REQUIRE(gpio.isSet());

    clock.advanceBy(FAST_INTERVAL_MS - 1);
    driver.update();
    REQUIRE(gpio.isSet());

    clock.advanceBy(1);
    driver.update();
    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("changing state resets the blink phase and re-stamps the toggle timer immediately")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    StatusLedDriver driver(gpio, clock, SLOW_INTERVAL_MS, FAST_INTERVAL_MS);

    driver.setState(StatusLedDriver::State::SlowBlink);
    clock.advanceBy(SLOW_INTERVAL_MS - 1);
    driver.update();
    REQUIRE(gpio.isSet()); // hasn't toggled yet under the old SlowBlink phase

    driver.setState(StatusLedDriver::State::FastBlink);
    REQUIRE(gpio.isSet()); // the transition itself re-shows "on" immediately

    // The fast-blink timer starts fresh from this transition, not from the
    // stale SlowBlink toggle time.
    clock.advanceBy(FAST_INTERVAL_MS - 1);
    driver.update();
    REQUIRE(gpio.isSet());

    clock.advanceBy(1);
    driver.update();
    REQUIRE_FALSE(gpio.isSet());
}

TEST_CASE("update() is a no-op while On or Off, never issuing a redundant GPIO write")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    StatusLedDriver driver(gpio, clock, SLOW_INTERVAL_MS, FAST_INTERVAL_MS);

    driver.setState(StatusLedDriver::State::On);
    const int setCallsAfterOn = gpio.setCallCount();

    driver.update();
    driver.update();
    driver.update();

    REQUIRE(gpio.setCallCount() == setCallsAfterOn);

    driver.setState(StatusLedDriver::State::Off);
    const int setCallsAfterOff = gpio.setCallCount();

    driver.update();
    driver.update();

    REQUIRE(gpio.setCallCount() == setCallsAfterOff);
}

TEST_CASE("redundant setState calls with the same state do not reset an in-progress blink's phase")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    StatusLedDriver driver(gpio, clock, SLOW_INTERVAL_MS, FAST_INTERVAL_MS);

    driver.setState(StatusLedDriver::State::SlowBlink);
    clock.advanceBy(SLOW_INTERVAL_MS / 2);

    driver.setState(StatusLedDriver::State::SlowBlink); // redundant - same state

    clock.advanceBy(SLOW_INTERVAL_MS / 2);
    driver.update();

    REQUIRE_FALSE(gpio.isSet()); // toggled right on schedule from the original activation
}

TEST_CASE("begin() re-stamps the toggle timer so a stale elapsed time doesn't cause an immediate toggle")
{
    FakeDigitalOutput gpio;
    FakeClock clock;
    StatusLedDriver driver(gpio, clock, SLOW_INTERVAL_MS, FAST_INTERVAL_MS);

    driver.setState(StatusLedDriver::State::SlowBlink);
    clock.advanceBy(SLOW_INTERVAL_MS * 10); // simulates a long gap between construction and begin()

    driver.begin();
    REQUIRE(gpio.isSet()); // begin() re-shows "on" and resets the phase, not a stale toggle

    clock.advanceBy(SLOW_INTERVAL_MS - 1);
    driver.update();
    REQUIRE(gpio.isSet());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run (via Bash/Git Bash, not PowerShell): `pio test -e native -f test_status_led_driver`
Expected: FAIL / build error — `StatusLedDriver` does not exist yet.

- [ ] **Step 3: Create the header**

Create `lib/McsEsp32/src/domain/StatusLedDriver.h`:

```cpp
#pragma once

#include "ports/Clock.h"
#include "ports/DigitalOutput.h"

// Drives one single-color status LED (one real GPIO) through four states.
// update() must be called on every loop iteration for blinking to work.
class StatusLedDriver
{
public:
    enum class State
    {
        Off,
        On,
        SlowBlink,
        FastBlink
    };

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

- [ ] **Step 4: Create the implementation**

Create `lib/McsEsp32/src/domain/StatusLedDriver.cpp`:

```cpp
#include "StatusLedDriver.h"

StatusLedDriver::StatusLedDriver(DigitalOutput& gpio, Clock& clock, const unsigned long slowBlinkIntervalMs,
                                   const unsigned long fastBlinkIntervalMs)
    : gpio_(gpio), clock_(clock), slowBlinkIntervalMs_(slowBlinkIntervalMs), fastBlinkIntervalMs_(fastBlinkIntervalMs)
{
    lastToggleTime_ = clock_.nowMilliseconds();
    applyGpio();
}

void StatusLedDriver::begin()
{
    lastToggleTime_ = clock_.nowMilliseconds();
    blinkShowingOn_ = true;
    applyGpio();
}

void StatusLedDriver::setState(const State state)
{
    if (state == state_)
    {
        return;
    }
    state_ = state;
    blinkShowingOn_ = true;
    lastToggleTime_ = clock_.nowMilliseconds();
    applyGpio();
}

void StatusLedDriver::update()
{
    if (state_ != State::SlowBlink && state_ != State::FastBlink)
    {
        return;
    }

    const unsigned long now = clock_.nowMilliseconds();
    const unsigned long intervalMs = state_ == State::FastBlink ? fastBlinkIntervalMs_ : slowBlinkIntervalMs_;
    if (now - lastToggleTime_ < intervalMs)
    {
        return;
    }

    blinkShowingOn_ = !blinkShowingOn_;
    lastToggleTime_ = now;
    applyGpio();
}

void StatusLedDriver::applyGpio()
{
    switch (state_)
    {
        case State::Off:
            gpio_.set(false);
            break;
        case State::On:
            gpio_.set(true);
            break;
        case State::SlowBlink:
        case State::FastBlink:
            gpio_.set(blinkShowingOn_);
            break;
    }
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_status_led_driver`
Expected: PASS, all 8 test cases.

- [ ] **Step 6: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: 55 suites passing (54 existing + this new one), zero failures.

- [ ] **Step 7: Commit**

```bash
git add lib/McsEsp32/src/domain/StatusLedDriver.h lib/McsEsp32/src/domain/StatusLedDriver.cpp test/test_status_led_driver/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add StatusLedDriver for node4 status/activity LEDs

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: `ActivityTrackingTurnoutCommandPort` adapter

**Files:**
- Create: `lib/McsEsp32/src/adapters/ActivityTrackingTurnoutCommandPort.h`
- Create: `test/test_activity_tracking_turnout_command_port/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutCommandPort::send(int, TurnoutPosition)` (`lib/McsCore/src/ports/TurnoutCommandPort.h`, unmodified), `IdentifyModeTimer::trigger()`/`isActive()` (`lib/McsEsp32/src/domain/IdentifyModeTimer.h`, unmodified — confirmed independent of Task 1).
- Produces: `ActivityTrackingTurnoutCommandPort(TurnoutCommandPort& inner, IdentifyModeTimer& activityTimer)`, implementing `TurnoutCommandPort` — consumed by Task 3.

- [ ] **Step 1: Write the failing tests**

Create `test/test_activity_tracking_turnout_command_port/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/ActivityTrackingTurnoutCommandPort.h"
#include "domain/IdentifyModeTimer.h"
#include "support/FakeClock.h"
#include "support/FakeTurnoutCommandPort.h"

namespace
{
    constexpr unsigned long ACTIVITY_DURATION_MS = 150;
}

TEST_CASE("send forwards the address and position unchanged to the inner port")
{
    FakeTurnoutCommandPort inner;
    FakeClock clock;
    IdentifyModeTimer activityTimer(clock, ACTIVITY_DURATION_MS);
    ActivityTrackingTurnoutCommandPort port(inner, activityTimer);

    port.send(7, TurnoutPosition::Thrown);

    REQUIRE(inner.sentCommands.size() == 1);
    REQUIRE(inner.sentCommands[0].address == 7);
    REQUIRE(inner.sentCommands[0].position == TurnoutPosition::Thrown);
}

TEST_CASE("send triggers the activity timer")
{
    FakeTurnoutCommandPort inner;
    FakeClock clock;
    IdentifyModeTimer activityTimer(clock, ACTIVITY_DURATION_MS);
    ActivityTrackingTurnoutCommandPort port(inner, activityTimer);

    REQUIRE_FALSE(activityTimer.isActive());

    port.send(7, TurnoutPosition::Thrown);

    REQUIRE(activityTimer.isActive());
}

TEST_CASE("a second send before the timer expires extends rather than stacks a separate activation")
{
    FakeTurnoutCommandPort inner;
    FakeClock clock;
    IdentifyModeTimer activityTimer(clock, ACTIVITY_DURATION_MS);
    ActivityTrackingTurnoutCommandPort port(inner, activityTimer);

    port.send(7, TurnoutPosition::Thrown);
    clock.advanceBy(ACTIVITY_DURATION_MS - 1);
    REQUIRE(activityTimer.isActive());

    port.send(7, TurnoutPosition::Closed); // re-triggers before expiry

    clock.advanceBy(ACTIVITY_DURATION_MS - 1); // would have expired from the first trigger alone
    REQUIRE(activityTimer.isActive()); // still active - the second send extended the window

    REQUIRE(inner.sentCommands.size() == 2);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_activity_tracking_turnout_command_port`
Expected: FAIL / build error — `ActivityTrackingTurnoutCommandPort` does not exist yet.

- [ ] **Step 3: Create the header (no `.cpp` — trivial forwarding, same shape as `GatedDigitalInput`'s precedent)**

Create `lib/McsEsp32/src/adapters/ActivityTrackingTurnoutCommandPort.h`:

```cpp
#pragma once

#include "../domain/IdentifyModeTimer.h"
#include "ports/TurnoutCommandPort.h"

// Decorates a TurnoutCommandPort, forwarding every send() to the wrapped
// port while also triggering an activity timer - lets a composition root
// flash an "activity" LED on outbound Loco2MQTT traffic without any change
// to the wrapped port or its callers.
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

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_activity_tracking_turnout_command_port`
Expected: PASS, all 3 test cases.

- [ ] **Step 5: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: 56 suites passing (54 original + Task 1's new suite + this one), zero failures.

- [ ] **Step 6: Commit**

```bash
git add lib/McsEsp32/src/adapters/ActivityTrackingTurnoutCommandPort.h test/test_activity_tracking_turnout_command_port/test_main.cpp
git commit -m "$(cat <<'EOF'
^ F Add ActivityTrackingTurnoutCommandPort decorator

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Wire both new classes into `src/esp32node4/main.cpp`

**Files:**
- Modify: `src/esp32node4/main.cpp`

**Interfaces:**
- Consumes: `StatusLedDriver` (Task 1), `ActivityTrackingTurnoutCommandPort` (Task 2), `IdentifyModeTimer` (pre-existing, already included in this file as of sub-project #9), `ArduinoDigitalOutput(int pin, bool activeLow)` (pre-existing).
- Produces: nothing consumed by a later task — Task 4 only documents this task's result.

The file currently reads (relevant excerpts — full current content was verified immediately before writing this plan, at commit `3b7387a`, with no intervening commits touching this file):

```cpp
#include "domain/IdentifyModeTimer.h"
#include "domain/LedPairDriver.h"
#include "domain/MacAddress.h"
```

```cpp
    constexpr unsigned long BROKER_RESOLVE_INTERVAL_MS = 60000;
    constexpr unsigned long IDENTIFY_DURATION_MS = 10000;
}
```

```cpp
const std::array<IndependentLedPairDriver*, 4> bootModeLedPairs = {
    &ledStations[0].driver(),
    &ledStations[1].driver(),
    &ledStations[2].driver(),
    &ledStations[3].driver(),
};
IndependentLedPairFlashIndicator<4> bootModeIndicator(bootModeLedPairs);

WiFiLink wifiLink(systemClock, RETRY_INTERVAL_MS);
```

```cpp
Loco2MqttTurnoutCommandAdapter4 turnoutCommandPort(mqttLink, runningConfig.channelTurnoutAddresses);
Loco2MqttFeedbackSource4 feedbackSource(mqttLink, runningConfig.channelTurnoutAddresses);

ToggleTurnoutStation stations[4] = {
    ToggleTurnoutStation(TURNOUT_CONFIGS[0].address, TURNOUT_CONFIGS[0].name, gatedButtons[0],
                          ledStations[0].green(), ledStations[0].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[1].address, TURNOUT_CONFIGS[1].name, gatedButtons[1],
                          ledStations[1].green(), ledStations[1].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[2].address, TURNOUT_CONFIGS[2].name, gatedButtons[2],
                          ledStations[2].green(), ledStations[2].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[3].address, TURNOUT_CONFIGS[3].name, gatedButtons[3],
                          ledStations[3].green(), ledStations[3].red(), systemClock, turnoutCommandPort),
};
```

```cpp
    for (auto& station : stations)
    {
        station.begin();
    }

    bootModeIndicator.show(bootMode);
```

```cpp
    if (configValid)
    {
        wifiLink.poll();
        if (wifiLink.connected())
        {
            mqttLink.poll();
        }
    }

    presenceAnnouncer.update(mqttLink.connected());

    if (configValid && mqttLink.connected() && !collision)
    {
        TurnoutFeedback feedback{};
        while (feedbackSource.poll(feedback))
        {
            for (auto& station : stations)
            {
                station.applyFeedback(feedback);
            }
        }
    }
    else
    {
        for (auto& station : stations)
        {
            station.clearIndicator();
        }
    }
```

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

- [ ] **Step 1: Add the new includes**

In the `domain/` include block, add `StatusLedDriver.h` alphabetically (after `SetupApName.h`, since `S < S` requires comparing the next letter — "SetupApName" < "StatusLedDriver" alphabetically):

```cpp
#include "domain/IdentifyModeTimer.h"
#include "domain/LedPairDriver.h"
#include "domain/MacAddress.h"
#include "domain/NodeConfig.h"
#include "domain/NodeIdentityGuard.h"
#include "domain/PresenceTopics.h"
#include "domain/SetupApName.h"
#include "domain/StatusLedDriver.h"
```

In the `adapters/` include block, add `ActivityTrackingTurnoutCommandPort.h` alphabetically (first entry, before `ArduinoClock.h`):

```cpp
#include "adapters/ActivityTrackingTurnoutCommandPort.h"
#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
```

- [ ] **Step 2: Add the new constants**

In the anonymous namespace holding the other constants:

```cpp
namespace
{
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
    constexpr unsigned long RETRY_INTERVAL_MS = 5000;
    constexpr unsigned long BROKER_RESOLVE_INTERVAL_MS = 60000;
    constexpr unsigned long IDENTIFY_DURATION_MS = 10000;
    constexpr int NETWORK_STATUS_LED_GPIO = 18;
    constexpr int ACTIVITY_LED_GPIO = 17;
    constexpr unsigned long NETWORK_LED_SLOW_BLINK_MS = 500;
    constexpr unsigned long NETWORK_LED_FAST_BLINK_MS = 150;
    constexpr unsigned long ACTIVITY_FLASH_DURATION_MS = 150;
}
```

- [ ] **Step 3: Add the new GPIO/driver/timer globals**

Immediately after the `bootModeIndicator` global, before `WiFiLink wifiLink(...)`:

```cpp
IndependentLedPairFlashIndicator<4> bootModeIndicator(bootModeLedPairs);

ArduinoDigitalOutput networkStatusLedGpio(NETWORK_STATUS_LED_GPIO, /*activeLow=*/false);
ArduinoDigitalOutput activityLedGpio(ACTIVITY_LED_GPIO, /*activeLow=*/false);
StatusLedDriver networkStatusLedDriver(networkStatusLedGpio, systemClock, NETWORK_LED_SLOW_BLINK_MS,
                                         NETWORK_LED_FAST_BLINK_MS);
IdentifyModeTimer activityFlashTimer(systemClock, ACTIVITY_FLASH_DURATION_MS);

WiFiLink wifiLink(systemClock, RETRY_INTERVAL_MS);
```

- [ ] **Step 4: Wrap `turnoutCommandPort` and update the 4 station constructions**

```cpp
Loco2MqttTurnoutCommandAdapter4 turnoutCommandPort(mqttLink, runningConfig.channelTurnoutAddresses);
Loco2MqttFeedbackSource4 feedbackSource(mqttLink, runningConfig.channelTurnoutAddresses);

ActivityTrackingTurnoutCommandPort activityTrackingCommandPort(turnoutCommandPort, activityFlashTimer);

ToggleTurnoutStation stations[4] = {
    ToggleTurnoutStation(TURNOUT_CONFIGS[0].address, TURNOUT_CONFIGS[0].name, gatedButtons[0],
                          ledStations[0].green(), ledStations[0].red(), systemClock, activityTrackingCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[1].address, TURNOUT_CONFIGS[1].name, gatedButtons[1],
                          ledStations[1].green(), ledStations[1].red(), systemClock, activityTrackingCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[2].address, TURNOUT_CONFIGS[2].name, gatedButtons[2],
                          ledStations[2].green(), ledStations[2].red(), systemClock, activityTrackingCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[3].address, TURNOUT_CONFIGS[3].name, gatedButtons[3],
                          ledStations[3].green(), ledStations[3].red(), systemClock, activityTrackingCommandPort),
};
```

- [ ] **Step 5: `begin()` the new GPIOs and driver in `setup()`**

```cpp
    for (auto& station : stations)
    {
        station.begin();
    }

    networkStatusLedGpio.begin();
    activityLedGpio.begin();
    networkStatusLedDriver.begin();

    bootModeIndicator.show(bootMode);
```

(`activityFlashTimer` needs no `begin()` call — `IdentifyModeTimer` has none, confirmed from its header.)

- [ ] **Step 6: Drive both LEDs in `loop()`**

Replace the existing block:

```cpp
    if (configValid)
    {
        wifiLink.poll();
        if (wifiLink.connected())
        {
            mqttLink.poll();
        }
    }

    presenceAnnouncer.update(mqttLink.connected());

    if (configValid && mqttLink.connected() && !collision)
    {
        TurnoutFeedback feedback{};
        while (feedbackSource.poll(feedback))
        {
            for (auto& station : stations)
            {
                station.applyFeedback(feedback);
            }
        }
    }
    else
    {
        for (auto& station : stations)
        {
            station.clearIndicator();
        }
    }
```

with:

```cpp
    if (configValid)
    {
        wifiLink.poll();
        if (wifiLink.connected())
        {
            mqttLink.poll();
        }
    }

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

    presenceAnnouncer.update(mqttLink.connected());

    if (configValid && mqttLink.connected() && !collision)
    {
        TurnoutFeedback feedback{};
        while (feedbackSource.poll(feedback))
        {
            activityFlashTimer.trigger();
            for (auto& station : stations)
            {
                station.applyFeedback(feedback);
            }
        }
    }
    else
    {
        for (auto& station : stations)
        {
            station.clearIndicator();
        }
    }
```

- [ ] **Step 7: Read the activity timer as the very last statement in `loop()`**

Replace the existing final block:

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
    for (auto& ledStation : ledStations)
    {
        ledStation.update();
    }

    for (auto& station : stations)
    {
        station.update();
    }

    // Must run after the station.update() loop above, where a same-tick send
    // actually reaches activityTrackingCommandPort and calls trigger().
    // Reading isActive() any earlier in this tick would show last tick's
    // state and delay a fresh flash by one tick - the same ordering hazard
    // sub-project #9's identify wiring documents for ledStation.update().
    activityLedGpio.set(activityFlashTimer.isActive());
}
```

- [ ] **Step 8: Build all 3 hardware environments**

```bash
pio run -e esp32node4
pio run -e esp32dev
pio run -e megaatmega2560
```

Expected: all three build clean (`SUCCESS`). `esp32dev`/`megaatmega2560` are expected to be byte-for-byte unaffected (no shared library code outside `lib/McsEsp32`'s two new files, which neither of those environments' composition roots reference) — their build is a regression check, not something this task's code could plausibly break, but it must still be run and reported.

- [ ] **Step 9: Run the native test suite**

```bash
pio test -e native
```

Expected: unaffected by this task specifically (56 suites passing, same as after Task 2 — this task adds no new test suite of its own, since `main.cpp` has no native test target).

**Do NOT launch this command with a backgrounding mechanism and then wait across turns for it.** Run it and wait for it to finish in the same turn.

- [ ] **Step 10: Commit**

```bash
git add src/esp32node4/main.cpp
git commit -m "$(cat <<'EOF'
@ F Wire status/activity LEDs into node4 main.cpp

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(`@` risk symbol: `main.cpp` composition-root change with no possible native test, same classification as every prior node4 `main.cpp`-only commit in this effort.)

---

### Task 4: Document sub-project #10 and run the staleness sweeps

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/pcb/turnout-node-4ch-pcb-design.md`
- Modify: `README.md` (if the staleness sweep finds anything this sub-project makes false there)

**Interfaces:**
- Consumes: Task 3's completed wiring (this task describes what Tasks 1-3 built; it does not touch code).
- Produces: nothing consumed by a later task — this is the last task in this plan.

- [ ] **Step 1: Add the new CLAUDE.md section**

Add a new `### Status/activity LEDs (sub-project #10)` section to `CLAUDE.md`, immediately after the "### Identify-overlay (sub-project #9)" section (which itself sits immediately before the unrelated, older "### Loco2MQTT turnout bridge (sub-project #9a)" section — re-read the live file to find the exact current boundary, since line numbers shift with every edit).

Use this exact text:

```markdown
### Status/activity LEDs (sub-project #10)

(2026-10-25, in this node-board effort's own numbering — not the unrelated,
older "#9a"/"#9b"/"#9c" panel-focused sub-project sequence referenced
elsewhere in this file.)

Closes the last open item in `docs/pcb/turnout-node-4ch-pcb-design.md`
§7 (item 5, "Status and activity LEDs"). Unlike every node4 sub-project
since #9, this one has no panel equivalent to mirror — the panel has no
on-board status/activity LEDs at all, only its 12 turnout LED pairs,
which is exactly the distinction the PCB doc draws ("this is also where
wireless-setup and WiFi/broker state get their own indicator **instead
of borrowing the turnout LEDs**"). Two new classes, both fully
native-tested:

- `StatusLedDriver` (`lib/McsEsp32/src/domain/`) — a new, generic
  single-GPIO LED driver with four states (`Off`/`On`/`SlowBlink`/
  `FastBlink`), mirroring `LedPairDriver`'s existing toggle-timing shape
  (re-stamping the toggle time on a state change so a transition takes
  visible effect immediately) but deliberately simpler: one color, one
  GPIO, and a genuine `Off` — `LedPairDriver`'s "exactly one color is
  always lit" invariant doesn't apply here and would have been the
  wrong class to reuse or extend. Drives the network status LED
  (GPIO 18): `src/esp32node4/main.cpp`'s `loop()` computes the desired
  state every tick — collision (highest priority) → `FastBlink`; no
  WiFi → `Off`; WiFi but no broker → `SlowBlink`; both connected → `On`
  — and calls `setState()`/`update()`.
- `ActivityTrackingTurnoutCommandPort` (`lib/McsEsp32/src/adapters/`,
  header-only, no `.cpp`) — a decorator implementing `TurnoutCommandPort`
  that forwards `send()` to a wrapped inner port while also triggering
  an injected `IdentifyModeTimer&`. Wraps `turnoutCommandPort` before it
  reaches the 4 `ToggleTurnoutStation`s — no change to
  `ToggleTurnoutStation`, `ToggleTurnoutControl`, or
  `Loco2MqttTurnoutCommandAdapter4`. This is the hexagonal-architecture
  way to observe outbound traffic at the composition-root boundary
  without touching any already-tested production class the panel also
  depends on.
- `IdentifyModeTimer` is reused **as-is, unmodified** for the activity
  LED's momentary flash (GPIO 17, 150ms) — it already provides exactly
  "trigger() stamps the clock, isActive() compares against a fixed
  duration, a second trigger() before expiry extends rather than
  stacks," with no identify-specific logic baked into its
  implementation. No new timer class was needed. The activity flash
  fires on every Loco2MQTT command sent (via the decorator above) or
  feedback received (one added `activityFlashTimer.trigger()` line
  inside the existing feedback-poll loop).

`src/esp32node4/main.cpp`'s activity-LED GPIO read is the **very last
statement in `loop()`**, after the `station.update()` loop where an
outbound send actually reaches the decorator — the same ordering
principle sub-project #9's identify wiring documents: a same-tick event
must be visible the same tick, not one tick later.

**`BootMode::WirelessSetup` needs no new branch.** WiFi/MQTT are never
started in that mode, so the network LED's state computation reads
`Off` naturally and no traffic ever exists to trigger the activity
flash — both LEDs simply stay off, by construction rather than by a
special case. The existing 4-turnout-LED flash (sub-project #5) remains
the sole setup-mode indicator, a deliberate scope decision made during
brainstorming rather than giving these two new LEDs a redundant pattern
of their own.

**Net effect:** closes the last open item in the PCB doc's firmware
checklist (§7) — every item is now done.

**Test coverage added:** `test/test_status_led_driver` (8 cases) and
`test/test_activity_tracking_turnout_command_port` (3 cases) — 54 → 56
suites. `src/esp32node4/main.cpp` itself has no native test target, same
precedent as every prior `main.cpp`-only change in this project.
```

- [ ] **Step 2: Run the general staleness sweep, widened to include README.md**

```bash
grep -n "future sub-project\|blocked on\|unwired\|not done\|still\|yet" CLAUDE.md docs/pcb/*.md README.md
```

Read every hit's surrounding context (not just the matched line) and annotate, in place, every claim this sub-project makes false — per the project's established convention, never delete a stale paragraph, revise it to state current reality and point at this section. Pay particular attention to:
- The PCB doc §7 item 5 bullet itself ("New outputs, with no existing firmware owner...") — needs a "— done (sub-project #10)" treatment matching items 1/4/6's existing style.
- Any remaining "status/activity LEDs...remain their own future sub-project" phrasing anywhere in CLAUDE.md (there are at least 2 such references as of sub-project #9's own section and the "Boot-mode LED indicator" section's non-goals — re-grep rather than trusting this count, since it may have drifted).
- `README.md` — re-read it in full (not just grep) given sub-project #9's final review found a staleness gap there that no keyword sweep had ever caught; confirm nothing it says about the node4 board's remaining work is made stale by this sub-project closing the status/activity LED item.

Leave any hit describing already-correct, still-true behavior untouched (e.g. unrelated hardware-verification checklist items, the panel's own description, or historical notes about already-closed sub-projects).

- [ ] **Step 3: Run the PCB-doc-specific staleness sweep**

```bash
grep -n "Status as of sub-project" docs/pcb/*.md
```

This matches §7's status line in `docs/pcb/turnout-node-4ch-pcb-design.md`. Update it to reference sub-project #10 and this sub-project's date, and — since this is the item that closes the **last** open §7 item — change its framing from "item N remains open" to a statement that all 7 items are now done. Extend both reference lists (spec file paths, CLAUDE.md section names) to include this sub-project's own files/section name:
- Add `docs/superpowers/specs/2026-10-25-esp32node4-status-activity-leds-design.md` to the spec-file list.
- Add `"Status/activity LEDs"` to the CLAUDE.md-section-name list.

- [ ] **Step 4: Update the §7 item 5 bullet**

In `docs/pcb/turnout-node-4ch-pcb-design.md`'s §7 (and §11's checklist, if it has a corresponding un-checked item for status/activity LEDs — re-read the live file to confirm whether one exists, since not every §7 item necessarily has a mirrored §11 bullet), mark item 5 done with a "— done (sub-project #10)" treatment and a resolution parenthetical matching the exact style items 1/4/6 already use (e.g. `*(Resolved via StatusLedDriver for the network LED and the reused IdentifyModeTimer plus a new ActivityTrackingTurnoutCommandPort decorator for the activity LED — see CLAUDE.md's "Status/activity LEDs" section.)*`).

- [ ] **Step 5: Verify all sweeps are clean**

Re-run all three greps from Steps 2-3. Confirm every hit that was made false by this sub-project's work is now annotated, and that no hit describing already-correct, still-true behavior was altered.

- [ ] **Step 6: Commit**

```bash
git add CLAUDE.md docs/pcb/turnout-node-4ch-pcb-design.md README.md
git commit -m "$(cat <<'EOF'
. d Document sub-project #10 status/activity LEDs

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(Only `git add README.md` if the sweep actually found something to change there — drop it from the add command otherwise.)
