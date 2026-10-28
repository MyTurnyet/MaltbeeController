#include <catch2/catch_test_macros.hpp>

#include "adapters/Loco2MqttFeedbackSource.h"
#include "testing/FakeMqttTransport.h"

namespace
{
    std::array<int, 4> addressesWithChannel(int channel, int address)
    {
        std::array<int, 4> addresses{};
        addresses[channel - 1] = address;
        return addresses;
    }
}

TEST_CASE("construction subscribes only the configured channels' state topics (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);

    Loco2MqttFeedbackSource4 source(transport, addresses);

    REQUIRE(transport.subscribedTopics.size() == 1);
    REQUIRE(transport.subscribedTopics[0] == "loconet/turnout/5/state");
}

TEST_CASE("construction subscribes each configured channel's state topic among several (N=4)")
{
    FakeMqttTransport transport;
    auto addresses = addressesWithChannel(1, 5);
    addresses[3] = 17;

    Loco2MqttFeedbackSource4 source(transport, addresses);

    REQUIRE(transport.subscribedTopics.size() == 2);
}

TEST_CASE("a valid incoming payload on the state topic becomes a pollable TurnoutFeedback (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(3, 9);
    Loco2MqttFeedbackSource4 source(transport, addresses);

    transport.deliver("loconet/turnout/9/state", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE(feedback.address == 3);
    REQUIRE(feedback.position == TurnoutPosition::Thrown);
}

TEST_CASE("an unrecognized payload on the state topic produces nothing (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource4 source(transport, addresses);

    transport.deliver("loconet/turnout/5/state", "GARBAGE");

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("a message on the command topic is not picked up as feedback (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource4 source(transport, addresses);

    transport.deliver("loconet/turnout/5/set", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("poll returns false once the queue is drained (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource4 source(transport, addresses);

    transport.deliver("loconet/turnout/5/state", "CLOSED");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("multiple queued messages drain in FIFO order (N=4)")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttFeedbackSource4 source(transport, addresses);

    transport.deliver("loconet/turnout/5/state", "CLOSED");
    transport.deliver("loconet/turnout/5/state", "THROWN");

    TurnoutFeedback first{};
    TurnoutFeedback second{};
    REQUIRE(source.poll(first));
    REQUIRE(source.poll(second));
    REQUIRE(first.position == TurnoutPosition::Closed);
    REQUIRE(second.position == TurnoutPosition::Thrown);
}
