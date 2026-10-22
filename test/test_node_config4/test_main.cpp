#include <catch2/catch_test_macros.hpp>

#include "domain/NodeConfig.h"

TEST_CASE("NodeConfig4 has a channel count of 4")
{
    REQUIRE(NodeConfig4::kChannelCount == 4);
}

TEST_CASE("NodeConfig4 factory default has 4 unconfigured channels")
{
    const NodeConfig4 config = NodeConfig4::factoryDefault();

    REQUIRE(config.channelTurnoutAddresses.size() == 4);
    for (const int address : config.channelTurnoutAddresses)
    {
        REQUIRE(address == 0);
    }
}

TEST_CASE("NodeConfig4 withChannelAddress accepts channel 4")
{
    const NodeConfig4 original = NodeConfig4::factoryDefault();

    const NodeConfig4 updated = original.withChannelAddress(4, 42);

    REQUIRE(updated.channelTurnoutAddresses[3] == 42);
}

TEST_CASE("NodeConfig4 withChannelAddress ignores channel 5, which was valid for the 12-channel panel")
{
    const NodeConfig4 original = NodeConfig4::factoryDefault();

    const NodeConfig4 updated = original.withChannelAddress(5, 42);

    for (const int address : updated.channelTurnoutAddresses)
    {
        REQUIRE(address == 0);
    }
}

TEST_CASE("NodeConfig4 validate rejects two channels claiming the same turnout address")
{
    const NodeConfig4 config = NodeConfig4::factoryDefault()
                                    .withNodeId(1)
                                    .withWifi("MyLayoutWifi", "hunter2")
                                    .withBroker("192.168.1.50", 1883)
                                    .withChannelAddress(1, 5)
                                    .withChannelAddress(2, 5);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("A fully valid NodeConfig4 with all 4 channels addressed passes validation")
{
    const NodeConfig4 config = NodeConfig4::factoryDefault()
                                    .withNodeId(1)
                                    .withWifi("MyLayoutWifi", "hunter2")
                                    .withBroker("192.168.1.50", 1883)
                                    .withChannelAddress(1, 1)
                                    .withChannelAddress(2, 2)
                                    .withChannelAddress(3, 3)
                                    .withChannelAddress(4, 4);

    REQUIRE(config.validate().empty());
}
