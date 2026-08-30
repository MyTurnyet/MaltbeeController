#include <catch2/catch_test_macros.hpp>

#include "domain/BootModeSelector.h"
#include "domain/NodeConfig.h"

namespace
{
    NodeConfig validConfig()
    {
        return NodeConfig::factoryDefault()
            .withNodeId(1)
            .withWifi("MyLayoutWifi", "hunter2")
            .withBroker("192.168.1.50", 1883);
    }
}

TEST_CASE("a pending wireless setup request selects WirelessSetup even with an invalid config")
{
    const NodeConfig config = NodeConfig::factoryDefault();

    REQUIRE(BootModeSelector::select(config, true) == BootMode::WirelessSetup);
}

TEST_CASE("a pending wireless setup request selects WirelessSetup even with a valid config")
{
    REQUIRE(BootModeSelector::select(validConfig(), true) == BootMode::WirelessSetup);
}

TEST_CASE("no request and a valid config selects Normal")
{
    REQUIRE(BootModeSelector::select(validConfig(), false) == BootMode::Normal);
}

TEST_CASE("no request and an invalid config selects NeedsCommissioning")
{
    const NodeConfig config = NodeConfig::factoryDefault();

    REQUIRE(BootModeSelector::select(config, false) == BootMode::NeedsCommissioning);
}
