#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/ActivityTrackingTurnoutCommandPort.h"
#include "domain/IdentifyModeTimer.h"
#include "testing/FakeClock.h"
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
