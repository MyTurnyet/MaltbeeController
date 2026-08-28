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

TEST_CASE("withChannelName sets the named channel (1-based) without mutating the original")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withChannelName(1, "LT1");

    REQUIRE(updated.channelJmriNames[0] == "LT1");
    REQUIRE(original.channelJmriNames[0].empty());
}

TEST_CASE("withChannelName ignores an out-of-range channel number")
{
    const NodeConfig original = NodeConfig::factoryDefault();

    const NodeConfig updated = original.withChannelName(13, "LT13");

    for (const auto& name : updated.channelJmriNames)
    {
        REQUIRE(name.empty());
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

TEST_CASE("A valid config with only some channels named still passes validation")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelName(1, "LT1")
                                   .withChannelName(2, "LT2");

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

TEST_CASE("validate rejects two channels claiming the same jmri name")
{
    const NodeConfig config = NodeConfig::factoryDefault()
                                   .withNodeId(1)
                                   .withWifi("MyLayoutWifi", "hunter2")
                                   .withBroker("192.168.1.50", 1883)
                                   .withChannelName(1, "LT1")
                                   .withChannelName(2, "LT1");

    REQUIRE_FALSE(config.validate().empty());
}
