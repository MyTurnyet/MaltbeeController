#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/ToggleTurnoutStation.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeDigitalOutput.h"
#include "support/FakeTurnoutCommandPort.h"

namespace
{
    constexpr unsigned long DEBOUNCE_MS = 30;
}

TEST_CASE("begin() clears the indicator even if an output was previously left on")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    FakeDigitalOutput closedOutput;
    FakeDigitalOutput thrownOutput;
    FakeTurnoutCommandPort commandPort;

    thrownOutput.set(true);

    ToggleTurnoutStation station(101, "Test Turnout", buttonInput, closedOutput, thrownOutput, clock, commandPort);
    station.begin();

    REQUIRE_FALSE(closedOutput.isSet());
    REQUIRE_FALSE(thrownOutput.isSet());
}

TEST_CASE("update() sends the opposite of the turnout's constructed-default position on a debounced press")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    FakeDigitalOutput closedOutput;
    FakeDigitalOutput thrownOutput;
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutStation station(101, "Test Turnout", buttonInput, closedOutput, thrownOutput, clock, commandPort);
    station.begin();

    buttonInput.active = true;
    station.update();
    clock.advanceBy(DEBOUNCE_MS);
    station.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
    REQUIRE(commandPort.sentCommands[0].address == 101);
    REQUIRE(commandPort.sentCommands[0].position == TurnoutPosition::Thrown);
}

TEST_CASE("applyFeedback for a matching address updates the turnout and lights the matching output")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    FakeDigitalOutput closedOutput;
    FakeDigitalOutput thrownOutput;
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutStation station(101, "Test Turnout", buttonInput, closedOutput, thrownOutput, clock, commandPort);
    station.begin();

    station.applyFeedback({101, TurnoutPosition::Thrown});

    REQUIRE(thrownOutput.isSet());
    REQUIRE_FALSE(closedOutput.isSet());
}

TEST_CASE("applyFeedback for a non-matching address leaves a previously-lit output unchanged")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    FakeDigitalOutput closedOutput;
    FakeDigitalOutput thrownOutput;
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutStation station(101, "Test Turnout", buttonInput, closedOutput, thrownOutput, clock, commandPort);
    station.begin();
    station.applyFeedback({101, TurnoutPosition::Thrown});

    station.applyFeedback({202, TurnoutPosition::Closed});

    REQUIRE(thrownOutput.isSet());
    REQUIRE_FALSE(closedOutput.isSet());
}

TEST_CASE("clearIndicator turns both outputs off even after a real applyFeedback lit one")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    FakeDigitalOutput closedOutput;
    FakeDigitalOutput thrownOutput;
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutStation station(101, "Test Turnout", buttonInput, closedOutput, thrownOutput, clock, commandPort);
    station.begin();
    station.applyFeedback({101, TurnoutPosition::Thrown});
    REQUIRE(thrownOutput.isSet());

    station.clearIndicator();

    REQUIRE_FALSE(closedOutput.isSet());
    REQUIRE_FALSE(thrownOutput.isSet());
}
