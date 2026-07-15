#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/TurnoutControl.h"
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

TEST_CASE("Throw-button press sends one thrown command")
{
    FakeDigitalInput throwInput;
    FakeDigitalInput closeInput;
    FakeClock clock;
    Button throwButton(throwInput, clock, DEBOUNCE_MS);
    Button closeButton(closeInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    pressButton(throwButton, throwInput, clock);
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
    REQUIRE(commandPort.sentCommands[0].address == 101);
    REQUIRE(commandPort.sentCommands[0].position == TurnoutPosition::Thrown);
}

TEST_CASE("Close-button press sends one closed command")
{
    FakeDigitalInput throwInput;
    FakeDigitalInput closeInput;
    FakeClock clock;
    Button throwButton(throwInput, clock, DEBOUNCE_MS);
    Button closeButton(closeInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Thrown, false, false);
    FakeTurnoutCommandPort commandPort;

    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    pressButton(closeButton, closeInput, clock);
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
    REQUIRE(commandPort.sentCommands[0].address == 101);
    REQUIRE(commandPort.sentCommands[0].position == TurnoutPosition::Closed);
}

TEST_CASE("Holding throw button sends no repeat commands")
{
    FakeDigitalInput throwInput;
    FakeDigitalInput closeInput;
    FakeClock clock;
    Button throwButton(throwInput, clock, DEBOUNCE_MS);
    Button closeButton(closeInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    pressButton(throwButton, throwInput, clock);
    control.update();

    clock.advanceBy(DEBOUNCE_MS);
    throwButton.update();
    control.update();

    REQUIRE(commandPort.sentCommands.size() == 1);
}

TEST_CASE("Feedback updates turnout state")
{
    FakeDigitalInput throwInput;
    FakeDigitalInput closeInput;
    FakeClock clock;
    Button throwButton(throwInput, clock, DEBOUNCE_MS);
    Button closeButton(closeInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({101, TurnoutPosition::Thrown});

    REQUIRE(turnout.position() == TurnoutPosition::Thrown);
}

TEST_CASE("Feedback updates indicators")
{
    FakeDigitalInput throwInput;
    FakeDigitalInput closeInput;
    FakeClock clock;
    Button throwButton(throwInput, clock, DEBOUNCE_MS);
    Button closeButton(closeInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({101, TurnoutPosition::Thrown});

    REQUIRE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
}

TEST_CASE("Feedback for another address is ignored")
{
    FakeDigitalInput throwInput;
    FakeDigitalInput closeInput;
    FakeClock clock;
    Button throwButton(throwInput, clock, DEBOUNCE_MS);
    Button closeButton(closeInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({202, TurnoutPosition::Thrown});

    REQUIRE(turnout.position() == TurnoutPosition::Closed);
    REQUIRE_FALSE(thrownIndicator.isOn());
}

TEST_CASE("Repeated feedback for the current state causes no harmful behavior")
{
    FakeDigitalInput throwInput;
    FakeDigitalInput closeInput;
    FakeClock clock;
    Button throwButton(throwInput, clock, DEBOUNCE_MS);
    Button closeButton(closeInput, clock, DEBOUNCE_MS);

    FakeDigitalOutput thrownOutput;
    FakeDigitalOutput closedOutput;
    Indicator thrownIndicator(thrownOutput);
    Indicator closedIndicator(closedOutput);
    TurnoutIndicator turnoutIndicator(thrownIndicator, closedIndicator);

    Turnout turnout(101, "Test Turnout", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;

    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    control.applyFeedback({101, TurnoutPosition::Thrown});
    control.applyFeedback({101, TurnoutPosition::Thrown});

    REQUIRE(turnout.position() == TurnoutPosition::Thrown);
    REQUIRE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
}
