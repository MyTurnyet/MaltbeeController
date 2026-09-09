#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/LedPairFlashIndicator.h"
#include "domain/LedPairDriver.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalOutput.h"

#include <array>

namespace
{
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
}

TEST_CASE("WirelessSetup starts the identify flash on every pair")
{
    FakeClock clock;
    FakeDigitalOutput gpioA;
    FakeDigitalOutput gpioB;
    LedPairDriver pairA(gpioA, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    LedPairDriver pairB(gpioB, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    std::array<LedPairDriver*, 2> pairs{&pairA, &pairB};
    LedPairFlashIndicator indicator(pairs);

    indicator.show(BootMode::WirelessSetup);

    REQUIRE(gpioA.isSet());
    REQUIRE(gpioB.isSet());
}

TEST_CASE("NeedsCommissioning starts the unconfirmed blink on every pair")
{
    FakeClock clock;
    FakeDigitalOutput gpioA;
    FakeDigitalOutput gpioB;
    LedPairDriver pairA(gpioA, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    LedPairDriver pairB(gpioB, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    std::array<LedPairDriver*, 2> pairs{&pairA, &pairB};
    LedPairFlashIndicator indicator(pairs);

    pairA.setGreen(true);
    pairB.setGreen(true);
    indicator.show(BootMode::NeedsCommissioning);
    clock.advanceBy(BLINK_INTERVAL_MS);
    pairA.update();
    pairB.update();

    REQUIRE_FALSE(gpioA.isSet());
    REQUIRE_FALSE(gpioB.isSet());
}

TEST_CASE("Normal leaves identifying off so later station ticks can drive the pairs")
{
    FakeClock clock;
    FakeDigitalOutput gpio;
    LedPairDriver pair(gpio, clock, BLINK_INTERVAL_MS, LedPairColor::Red);
    std::array<LedPairDriver*, 1> pairs{&pair};
    LedPairFlashIndicator indicator(pairs);

    indicator.show(BootMode::WirelessSetup);
    indicator.show(BootMode::Normal);
    pair.setGreen(true);

    REQUIRE(gpio.isSet());
}
