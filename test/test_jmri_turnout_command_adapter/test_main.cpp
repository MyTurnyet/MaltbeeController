#include <catch2/catch_test_macros.hpp>

#include "adapters/JmriTurnoutCommandAdapter.h"
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

TEST_CASE("send publishes the expected topic, payload, and retained flag")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriTurnoutCommandAdapter adapter(transport, names);

    adapter.send(1, TurnoutPosition::Closed);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "track/turnout/LT1");
    REQUIRE(transport.published[0].payload == "CLOSED");
    REQUIRE_FALSE(transport.published[0].retained);
}

TEST_CASE("send encodes Thrown correctly")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriTurnoutCommandAdapter adapter(transport, names);

    adapter.send(1, TurnoutPosition::Thrown);

    REQUIRE(transport.published[0].payload == "THROWN");
}

TEST_CASE("send resolves the correct channel out of several configured")
{
    FakeMqttTransport transport;
    auto names = namesWithChannel(1, "LT1");
    names[4] = "LT5";
    JmriTurnoutCommandAdapter adapter(transport, names);

    adapter.send(5, TurnoutPosition::Closed);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "track/turnout/LT5");
}

TEST_CASE("send is a no-op for an unconfigured channel")
{
    FakeMqttTransport transport;
    const std::array<std::string, NodeConfig::kChannelCount> names{};
    JmriTurnoutCommandAdapter adapter(transport, names);

    adapter.send(1, TurnoutPosition::Closed);

    REQUIRE(transport.published.empty());
}

TEST_CASE("send is a no-op for an out-of-range channel")
{
    FakeMqttTransport transport;
    const auto names = namesWithChannel(1, "LT1");
    JmriTurnoutCommandAdapter adapter(transport, names);

    adapter.send(0, TurnoutPosition::Closed);
    adapter.send(NodeConfig::kChannelCount + 1, TurnoutPosition::Closed);

    REQUIRE(transport.published.empty());
}
