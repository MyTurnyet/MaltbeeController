#include <catch2/catch_test_macros.hpp>

#include "adapters/JmriFeedbackSource.h"
#include "domain/NodeConfig.h"
#include "support/FakeMqttTransport.h"

namespace
{
    std::array<std::string, NodeConfig::kChannelCount> namesWithChannel(int channel, const std::string& name)
    {
        std::array<std::string, NodeConfig::kChannelCount> names;
        names[channel - 1] = name;
        return names;
    }
}

TEST_CASE("construction subscribes only the configured channels' topics")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");

    JmriFeedbackSource source(transport, names);

    REQUIRE(transport.subscribedTopics.size() == 1);
    REQUIRE(transport.subscribedTopics[0] == "track/turnout/LT1");
}

TEST_CASE("construction subscribes each configured channel among several")
{
    FakeMqttTransport transport;
    auto names = namesWithChannel(1, "LT1");
    names[4] = "LT5";

    JmriFeedbackSource source(transport, names);

    REQUIRE(transport.subscribedTopics.size() == 2);
}

TEST_CASE("a valid incoming payload becomes a pollable TurnoutFeedback")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(3, "LT3");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT3", "THROWN");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE(feedback.address == 3);
    REQUIRE(feedback.position == TurnoutPosition::Thrown);
}

TEST_CASE("an unrecognized payload produces nothing")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT1", "GARBAGE");

    TurnoutFeedback feedback{};
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("poll returns false once the queue is drained")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT1", "CLOSED");

    TurnoutFeedback feedback{};
    REQUIRE(source.poll(feedback));
    REQUIRE_FALSE(source.poll(feedback));
}

TEST_CASE("multiple queued messages drain in FIFO order")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriFeedbackSource source(transport, names);

    transport.deliver("track/turnout/LT1", "CLOSED");
    transport.deliver("track/turnout/LT1", "THROWN");

    TurnoutFeedback first{};
    TurnoutFeedback second{};
    REQUIRE(source.poll(first));
    REQUIRE(source.poll(second));
    REQUIRE(first.position == TurnoutPosition::Closed);
    REQUIRE(second.position == TurnoutPosition::Thrown);
}
