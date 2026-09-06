#pragma once

#include <array>
#include <string>
#include <vector>

struct NodeConfig
{
    static constexpr int kChannelCount = 12;
    static constexpr int kMinNodeId = 1;
    static constexpr int kMaxNodeId = 99;
    static constexpr int kMinBrokerPort = 1;
    static constexpr int kMaxBrokerPort = 65535;
    static constexpr int kMinTurnoutAddress = 1;
    static constexpr int kMaxTurnoutAddress = 2048;

    int nodeId = 0;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    int brokerPort = 1883;
    std::array<int, kChannelCount> channelTurnoutAddresses{};

    static NodeConfig factoryDefault();

    [[nodiscard]] NodeConfig withNodeId(int id) const;
    [[nodiscard]] NodeConfig withWifi(std::string ssid, std::string password) const;
    [[nodiscard]] NodeConfig withBroker(std::string host, int port) const;
    [[nodiscard]] NodeConfig withChannelAddress(int channel, int address) const;

    [[nodiscard]] std::vector<std::string> validate() const;
};
