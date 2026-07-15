#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutIndicator.h"
#include "domain/Indicator.h"
#include "support/FakeDigitalOutput.h"

TEST_CASE("Displaying thrown turns on the thrown indicator")
{
    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    turnoutIndicator.display(TurnoutPosition::Thrown);

    REQUIRE(thrownIndicator.isOn());
}

TEST_CASE("Displaying thrown turns off the closed indicator")
{
    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    closedIndicator.on();
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    turnoutIndicator.display(TurnoutPosition::Thrown);

    REQUIRE_FALSE(closedIndicator.isOn());
}

TEST_CASE("Displaying closed turns on the closed indicator")
{
    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    turnoutIndicator.display(TurnoutPosition::Closed);

    REQUIRE(closedIndicator.isOn());
}

TEST_CASE("Displaying closed turns off the thrown indicator")
{
    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    thrownIndicator.on();
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    turnoutIndicator.display(TurnoutPosition::Closed);

    REQUIRE_FALSE(thrownIndicator.isOn());
}

TEST_CASE("Clear turns both indicators off")
{
    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    thrownIndicator.on();
    closedIndicator.on();
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    turnoutIndicator.clear();

    REQUIRE_FALSE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
}
