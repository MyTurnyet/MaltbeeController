#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/IndependentLedPairFlashIndicator.h"
#include "domain/IndependentLedPairDriver.h"
#include "testing/FakeClock.h"
#include "support/FakeDigitalOutput.h"

#include <array>

namespace
{
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
}

TEST_CASE("WirelessSetup starts the identify flash on every pair")
{
    FakeClock clock;
    FakeDigitalOutput greenGpioA;
    FakeDigitalOutput redGpioA;
    FakeDigitalOutput greenGpioB;
    FakeDigitalOutput redGpioB;
    IndependentLedPairDriver pairA(greenGpioA, redGpioA, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    IndependentLedPairDriver pairB(greenGpioB, redGpioB, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    std::array<IndependentLedPairDriver*, 2> pairs{&pairA, &pairB};
    IndependentLedPairFlashIndicator indicator(pairs);

    indicator.show(BootMode::WirelessSetup);

    REQUIRE(greenGpioA.isSet());
    REQUIRE_FALSE(redGpioA.isSet());
    REQUIRE(greenGpioB.isSet());
    REQUIRE_FALSE(redGpioB.isSet());
}

TEST_CASE("NeedsCommissioning starts the unconfirmed blink on every pair")
{
    FakeClock clock;
    FakeDigitalOutput greenGpioA;
    FakeDigitalOutput redGpioA;
    FakeDigitalOutput greenGpioB;
    FakeDigitalOutput redGpioB;
    IndependentLedPairDriver pairA(greenGpioA, redGpioA, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    IndependentLedPairDriver pairB(greenGpioB, redGpioB, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    std::array<IndependentLedPairDriver*, 2> pairs{&pairA, &pairB};
    IndependentLedPairFlashIndicator indicator(pairs);

    pairA.setGreen(true);
    pairB.setGreen(true);
    indicator.show(BootMode::NeedsCommissioning);
    clock.advanceBy(BLINK_INTERVAL_MS);
    pairA.update();
    pairB.update();

    REQUIRE_FALSE(greenGpioA.isSet());
    REQUIRE(redGpioA.isSet());
    REQUIRE_FALSE(greenGpioB.isSet());
    REQUIRE(redGpioB.isSet());
}

TEST_CASE("Normal leaves identifying off so later station ticks can drive the pairs")
{
    FakeClock clock;
    FakeDigitalOutput greenGpio;
    FakeDigitalOutput redGpio;
    IndependentLedPairDriver pair(greenGpio, redGpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    std::array<IndependentLedPairDriver*, 1> pairs{&pair};
    IndependentLedPairFlashIndicator indicator(pairs);

    indicator.show(BootMode::WirelessSetup);
    indicator.show(BootMode::Normal);
    pair.setGreen(true);

    REQUIRE(greenGpio.isSet());
    REQUIRE_FALSE(redGpio.isSet());
}
