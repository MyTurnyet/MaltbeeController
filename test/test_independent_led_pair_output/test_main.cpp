#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/IndependentLedPairOutput.h"
#include "domain/IndependentLedPairDriver.h"
#include "testing/FakeClock.h"
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
