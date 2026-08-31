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
