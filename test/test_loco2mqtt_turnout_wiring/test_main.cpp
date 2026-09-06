#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/Loco2MqttFeedbackSource.h"
#include "adapters/Loco2MqttTurnoutCommandAdapter.h"
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
    constexpr int TURNOUT_ADDRESS = 5;

    std::array<int, NodeConfig::kChannelCount> addressesWithChannel(int channel, int address)
    {
        std::array<int, NodeConfig::kChannelCount> addresses{};
        addresses[channel - 1] = address;
        return addresses;
    }

    void pressButton(Button& button, FakeDigitalInput& input, FakeClock& clock)
    {
        input.active = true;
        button.update();
        clock.advanceBy(DEBOUNCE_MS);
        button.update();
    }
}

TEST_CASE("a button press publishes a command on the set topic that is not picked up as feedback")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(CHANNEL, TURNOUT_ADDRESS);
    Loco2MqttTurnoutCommandAdapter commandAdapter(transport, addresses);
    Loco2MqttFeedbackSource feedbackSource(transport, addresses);

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

    Turnout turnout(CHANNEL, "T1", TurnoutPosition::Closed, false, false);
    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandAdapter);

    pressButton(throwButton, throwInput, clock);
    control.update();

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "loconet/turnout/5/set");
    REQUIRE(transport.published[0].payload == "THROWN");

    // The command and state topics are structurally distinct
    // (".../set" vs ".../state") - even delivering the exact outgoing
    // command message back on its own topic must never be picked up as
    // feedback, since the feedback source only ever subscribed the state
    // topic.
    transport.deliver(transport.published[0].topic, transport.published[0].payload);

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(feedbackSource.poll(feedback));
    REQUIRE(turnout.position() == TurnoutPosition::Closed);
    REQUIRE_FALSE(thrownIndicator.isOn());
}

TEST_CASE("Loco2MQTT's real state-topic confirmation updates the turnout and indicator")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(CHANNEL, TURNOUT_ADDRESS);
    Loco2MqttFeedbackSource feedbackSource(transport, addresses);

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

    Turnout turnout(CHANNEL, "T1", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;
    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    transport.deliver("loconet/turnout/5/state", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE(feedbackSource.poll(feedback));
    REQUIRE(feedback.address == CHANNEL);
    REQUIRE(feedback.position == TurnoutPosition::Thrown);

    control.applyFeedback(feedback);

    REQUIRE(turnout.position() == TurnoutPosition::Thrown);
    REQUIRE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
    REQUIRE(commandPort.sentCommands.empty());
}

TEST_CASE("a repeated identical state re-publish (Loco2MQTT's 30s heartbeat) is a harmless no-op")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(CHANNEL, TURNOUT_ADDRESS);
    Loco2MqttFeedbackSource feedbackSource(transport, addresses);

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

    Turnout turnout(CHANNEL, "T1", TurnoutPosition::Closed, false, false);
    FakeTurnoutCommandPort commandPort;
    TurnoutControl control(throwButton, closeButton, turnout, turnoutIndicator, commandPort);

    transport.deliver("loconet/turnout/5/state", "THROWN");
    TurnoutFeedback first{};
    REQUIRE(feedbackSource.poll(first));
    control.applyFeedback(first);

    transport.deliver("loconet/turnout/5/state", "THROWN");
    TurnoutFeedback second{};
    REQUIRE(feedbackSource.poll(second));
    control.applyFeedback(second);

    REQUIRE(turnout.position() == TurnoutPosition::Thrown);
    REQUIRE(thrownIndicator.isOn());
    REQUIRE_FALSE(closedIndicator.isOn());
}
