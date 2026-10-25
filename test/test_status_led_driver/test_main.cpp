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
