#include <catch2/catch_test_macros.hpp>

#include "adapters/Loco2MqttTurnoutCommandAdapter.h"
#include "domain/NodeConfig.h"
#include "support/FakeMqttTransport.h"

namespace
{
    std::array<int, NodeConfig::kChannelCount> addressesWithChannel(int channel, int address)
    {
        std::array<int, NodeConfig::kChannelCount> addresses{};
        addresses[channel - 1] = address;
        return addresses;
    }
}

TEST_CASE("send publishes the expected topic, payload, and retained flag")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttTurnoutCommandAdapter adapter(transport, addresses);

    adapter.send(1, TurnoutPosition::Closed);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "loconet/turnout/5/set");
    REQUIRE(transport.published[0].payload == "CLOSED");
    REQUIRE_FALSE(transport.published[0].retained);
}

TEST_CASE("send encodes Thrown correctly")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttTurnoutCommandAdapter adapter(transport, addresses);

    adapter.send(1, TurnoutPosition::Thrown);

    REQUIRE(transport.published[0].payload == "THROWN");
}

TEST_CASE("send resolves the correct channel out of several configured")
{
    FakeMqttTransport transport;
    auto addresses = addressesWithChannel(1, 5);
    addresses[4] = 17;
    Loco2MqttTurnoutCommandAdapter adapter(transport, addresses);

    adapter.send(5, TurnoutPosition::Closed);

    REQUIRE(transport.published.size() == 1);
    REQUIRE(transport.published[0].topic == "loconet/turnout/17/set");
}

TEST_CASE("send is a no-op for an unconfigured channel")
{
    FakeMqttTransport transport;
    const std::array<int, NodeConfig::kChannelCount> addresses{};
    Loco2MqttTurnoutCommandAdapter adapter(transport, addresses);

    adapter.send(1, TurnoutPosition::Closed);

    REQUIRE(transport.published.empty());
}

TEST_CASE("send is a no-op for an out-of-range channel")
{
    FakeMqttTransport transport;
    const auto addresses = addressesWithChannel(1, 5);
    Loco2MqttTurnoutCommandAdapter adapter(transport, addresses);

    adapter.send(0, TurnoutPosition::Closed);
    adapter.send(NodeConfig::kChannelCount + 1, TurnoutPosition::Closed);

    REQUIRE(transport.published.empty());
}
