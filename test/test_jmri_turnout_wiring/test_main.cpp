#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
#include "application/TurnoutControl.h"
#include "domain/Button.h"
#include "domain/Indicator.h"
#include "domain/NodeConfig.h"
#include "domain/Turnout.h"
#include "domain/TurnoutIndicator.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeDigitalOutput.h"
#include "support/FakeMqttTransport.h"
#include "support/FakeTurnoutCommandPort.h"

namespace
{
    constexpr unsigned long DEBOUNCE_MS = 30;
    constexpr int CHANNEL = 1;

    std::array<std::string, NodeConfig::kChannelCount> namesWithChannel(int channel, const std::string& name)
    {
        std::array<std::string, NodeConfig::kChannelCount> names;
        names[channel - 1] = name;
        return names;
    }

    void pressButton(Button& button, FakeDigitalInput& input, FakeClock& clock)
    {
        input.active = true;
        button.update();
        clock.advanceBy(DEBOUNCE_MS);
        button.update();
    }
}

TEST_CASE("a button press publishes a command that does not leak back as feedback, even when the broker echoes it verbatim")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(CHANNEL, "LT1");
    JmriTurnoutCommandAdapter commandAdapter(transport, names);
    JmriFeedbackSource feedbackSource(transport, names);

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

    Turnout turnout(CHANNEL, "LT1", TurnoutPosition::Closed, false, false);
    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandAdapter);

    pressButton(throwButton, throwInput, clock);
    control.update();

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "track/turnout/LT1");
    REQUIRE(transport.published[0].payload == "THROWN");

    // Simulate a real MQTT broker faithfully echoing the panel's own
    // publish back to it, exactly as MQTT 3.1.1 does with no "No Local"
    // option available.
    transport.deliver(transport.published[0].topic, transport.published[0].payload);

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(feedbackSource.poll(feedback));
    REQUIRE(turnout.position() == TurnoutPosition::Closed);
    REQUIRE_FALSE(thrownIndicator.isOn());
}

TEST_CASE("the real driver's confirmation on the dedicated state topic updates the turnout and indicator")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(CHANNEL, "LT1");
    JmriFeedbackSource feedbackSource(transport, names);

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

    Turnout turnout(CHANNEL, "LT1", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;
    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    transport.deliver("track/turnout/LT1/state", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE(feedbackSource.poll(feedback));
    REQUIRE(feedback.address == CHANNEL);
    REQUIRE(feedback.position == TurnoutPosition::Thrown);

    control.applyFeedback(feedback);

    REQUIRE(turnout.position() == TurnoutPosition::Thrown);
    REQUIRE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
}
