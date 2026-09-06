#include <catch2/catch_test_macros.hpp>

#include "domain/NodeConfig.h"

TEST_CASE("Factory-default NodeConfig fails validation")
{
    const NodeConfig config = NodeConfig::factoryDefault();

    REQUIRE(config.nodeId == 0);
    REQUIRE(config.wifiSsid.empty());
    REQUIRE(config.brokerHost.empty());
    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("withNodeId returns a modified copy without mutating the original")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withNodeId(5);

    REQUIRE(updated.nodeId == 5);
    REQUIRE(original.nodeId == 0);
}

TEST_CASE("withWifi returns a modified copy without mutating the original")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withWifi("MyLayoutWifi", "hunter2");

    REQUIRE(updated.wifiSsid == "MyLayoutWifi");
    REQUIRE(updated.wifiPassword == "hunter2");
    REQUIRE(original.wifiSsid.empty());
}

TEST_CASE("withBroker returns a modified copy without mutating the original")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withBroker("192.168.1.50", 1883);

    REQUIRE(updated.brokerHost == "192.168.1.50");
    REQUIRE(updated.brokerPort == 1883);
    REQUIRE(original.brokerHost.empty());
}

TEST_CASE("withChannelAddress sets the addressed channel (1-based) without mutating the original")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withChannelAddress(1, 5);

    REQUIRE(updated.channelTurnoutAddresses[0] == 5);
    REQUIRE(original.channelTurnoutAddresses[0] == 0);
}

TEST_CASE("withChannelAddress ignores an out-of-range channel number")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withChannelAddress(13, 5);

    for (const int address : updated.channelTurnoutAddresses)
    {
        REQUIRE(address == 0);
    }
}

TEST_CASE("A fully valid config passes validation")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883);

    REQUIRE(config.validate().empty());
}

TEST_CASE("A valid config with only some channels addressed still passes validation")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelAddress(1, 5)
                                   .withChannelAddress(2, 6);

    REQUIRE(config.validate().empty());
}

TEST_CASE("validate rejects a node id outside 1-99")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(0)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate rejects an empty wifi ssid")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withBroker("192.168.1.50", 1883);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate rejects an empty broker host")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2");

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate rejects a broker port outside 1-65535")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 0);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate rejects two channels claiming the same turnout address")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelAddress(1, 5)
                                   .withChannelAddress(2, 5);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate accepts the address range boundaries 1 and 2048")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelAddress(1, 1)
                                   .withChannelAddress(2, 2048);

    REQUIRE(config.validate().empty());
}

TEST_CASE("validate rejects a channel address below 1")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelAddress(1, -1);

    REQUIRE_FALSE(config.validate().empty());
}

TEST_CASE("validate rejects a channel address above 2048")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelAddress(1, 2049);

    REQUIRE_FALSE(config.validate().empty());
}
