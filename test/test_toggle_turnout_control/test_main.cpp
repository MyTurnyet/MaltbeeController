#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/ToggleTurnoutControl.h"
#include "domain/Button.h"
#include "domain/Indicator.h"
#include "domain/Turnout.h"
#include "domain/TurnoutIndicator.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeDigitalOutput.h"
#include "support/FakeTurnoutCommandPort.h"

namespace
{
    constexpr unsigned long DEBOUNCE_MS = 30;

    void pressButton(Button& button, FakeDigitalInput& input, FakeClock& clock)
    {
        input.active = true;
        button.update();
        clock.advanceBy(DEBOUNCE_MS);
        button.update();
    }
}

TEST_CASE("A press sends the opposite of the turnout's constructed-default position")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    pressButton(button, buttonInput, clock);
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
    REQUIRE(commandPort.sentCommands[0].address == 101);
    REQUIRE(commandPort.sentCommands[0].position == TurnoutPosition::Thrown);
}

TEST_CASE("After feedback confirms a position, the next press sends the opposite of that confirmed position")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({101, TurnoutPosition::Thrown});

    pressButton(button, buttonInput, clock);
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
    REQUIRE(commandPort.sentCommands[0].position == TurnoutPosition::Closed);
}

TEST_CASE("Repeated presses with no intervening feedback send the identical command each time")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    pressButton(button, buttonInput, clock);
    control.update();

    buttonInput.active = false;
    button.update();
    clock.advanceBy(DEBOUNCE_MS);
    button.update();
    pressButton(button, buttonInput, clock);
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 2);
    REQUIRE(commandPort.sentCommands[0].position == TurnoutPosition::Thrown);
    REQUIRE(commandPort.sentCommands[1].position == TurnoutPosition::Thrown);
}

TEST_CASE("Holding the button sends no repeat commands")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    pressButton(button, buttonInput, clock);
    control.update();

    clock.advanceBy(DEBOUNCE_MS);
    button.update();
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
}

TEST_CASE("Feedback for a matching address updates the turnout position and indicator, closed to thrown")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({101, TurnoutPosition::Thrown});

    REQUIRE(turnout.position() == TurnoutPosition::Thrown);
    REQUIRE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
}

TEST_CASE("Feedback for a matching address updates the turnout position and indicator, thrown to closed")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Thrown, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({101, TurnoutPosition::Closed});

    REQUIRE(turnout.position() == TurnoutPosition::Closed);
    REQUIRE(closedIndicator.isOn());
    REQUIRE_FALSE(thrownIndicator.isOn());
}

TEST_CASE("Feedback for a non-matching address is ignored")
{
    FakeDigitalInput buttonInput;
    FakeClock clock;
    Button button(buttonInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    ToggleTurnoutControl control(button, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({202, TurnoutPosition::Thrown});

    REQUIRE(turnout.position() == TurnoutPosition::Closed);
    REQUIRE_FALSE(thrownIndicator.isOn());
}
