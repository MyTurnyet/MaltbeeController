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
