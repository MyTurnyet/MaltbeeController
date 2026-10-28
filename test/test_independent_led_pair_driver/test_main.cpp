#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/IndependentLedPairDriver.h"
#include "testing/FakeClock.h"
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
