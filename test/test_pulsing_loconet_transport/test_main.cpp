#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/PulsingLocoNetTransport.h"
#include "support/FakeClock.h"
#include "support/FakeLocoNetSwitchDriver.h"

namespace
{
    constexpr unsigned long PULSE_DURATION_MS = 200;
}

TEST_CASE("sendPacket sends an immediate ON request with the packet's address and position")
{
    FakeLocoNetSwitchDriver driver;
    FakeClock clock;
    PulsingLocoNetTransport transport(driver, clock, PULSE_DURATION_MS);

    transport.sendPacket({101, TurnoutPosition::Thrown});

    REQUIRE(driver.requests.size() == 1);
    REQUIRE(driver.requests[0].address == 101);
    REQUIRE(driver.requests[0].position == TurnoutPosition::Thrown);
    REQUIRE(driver.requests[0].outputOn == true);
}

TEST_CASE("update() before the pulse duration has elapsed sends no additional request")
{
    FakeLocoNetSwitchDriver driver;
    FakeClock clock;
    PulsingLocoNetTransport transport(driver, clock, PULSE_DURATION_MS);

    transport.sendPacket({101, TurnoutPosition::Thrown});
    clock.advanceBy(PULSE_DURATION_MS - 1);
    transport.update();

    REQUIRE(driver.requests.size() == 1);
}

TEST_CASE("update() once the pulse duration has elapsed sends an OFF request with the same address and position")
{
    FakeLocoNetSwitchDriver driver;
    FakeClock clock;
    PulsingLocoNetTransport transport(driver, clock, PULSE_DURATION_MS);

    transport.sendPacket({101, TurnoutPosition::Thrown});
    clock.advanceBy(PULSE_DURATION_MS);
    transport.update();

    REQUIRE(driver.requests.size() == 2);
    REQUIRE(driver.requests[1].address == 101);
    REQUIRE(driver.requests[1].position == TurnoutPosition::Thrown);
    REQUIRE(driver.requests[1].outputOn == false);
}

TEST_CASE("update() does not resend the OFF request after it has already been sent")
{
    FakeLocoNetSwitchDriver driver;
    FakeClock clock;
    PulsingLocoNetTransport transport(driver, clock, PULSE_DURATION_MS);

    transport.sendPacket({101, TurnoutPosition::Thrown});
    clock.advanceBy(PULSE_DURATION_MS);
    transport.update();
    clock.advanceBy(PULSE_DURATION_MS);
    transport.update();

    REQUIRE(driver.requests.size() == 2);
}

TEST_CASE("sending a new packet while a release is still pending sends the previous OFF immediately, then the new ON")
{
    FakeLocoNetSwitchDriver driver;
    FakeClock clock;
    PulsingLocoNetTransport transport(driver, clock, PULSE_DURATION_MS);

    transport.sendPacket({101, TurnoutPosition::Thrown});
    clock.advanceBy(PULSE_DURATION_MS - 1);
    transport.sendPacket({202, TurnoutPosition::Closed});

    REQUIRE(driver.requests.size() == 3);
    REQUIRE(driver.requests[1].address == 101);
    REQUIRE(driver.requests[1].position == TurnoutPosition::Thrown);
    REQUIRE(driver.requests[1].outputOn == false);
    REQUIRE(driver.requests[2].address == 202);
    REQUIRE(driver.requests[2].position == TurnoutPosition::Closed);
    REQUIRE(driver.requests[2].outputOn == true);
}

TEST_CASE("two full pulses in sequence each drive their own ON and OFF correctly")
{
    FakeLocoNetSwitchDriver driver;
    FakeClock clock;
    PulsingLocoNetTransport transport(driver, clock, PULSE_DURATION_MS);

    transport.sendPacket({101, TurnoutPosition::Thrown});
    clock.advanceBy(PULSE_DURATION_MS);
    transport.update();

    transport.sendPacket({202, TurnoutPosition::Closed});
    clock.advanceBy(PULSE_DURATION_MS);
    transport.update();

    REQUIRE(driver.requests.size() == 4);
    REQUIRE(driver.requests[0].address == 101);
    REQUIRE(driver.requests[0].outputOn == true);
    REQUIRE(driver.requests[1].address == 101);
    REQUIRE(driver.requests[1].outputOn == false);
    REQUIRE(driver.requests[2].address == 202);
    REQUIRE(driver.requests[2].outputOn == true);
    REQUIRE(driver.requests[3].address == 202);
    REQUIRE(driver.requests[3].outputOn == false);
}

TEST_CASE("update() with no packet ever sent does nothing")
{
    FakeLocoNetSwitchDriver driver;
    FakeClock clock;
    PulsingLocoNetTransport transport(driver, clock, PULSE_DURATION_MS);

    clock.advanceBy(PULSE_DURATION_MS);
    transport.update();

    REQUIRE(driver.requests.empty());
}
