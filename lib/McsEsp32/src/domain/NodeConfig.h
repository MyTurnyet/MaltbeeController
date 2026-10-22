#pragma once

#include <array>
#include <string>
#include <vector>

template <int N>
struct BasicNodeConfig
{
    static constexpr int kChannelCount = N;
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

    static BasicNodeConfig factoryDefault()
    {
        return BasicNodeConfig{};
    }

    [[nodiscard]] BasicNodeConfig withNodeId(const int id) const
    {
        BasicNodeConfig copy = *this;
        copy.nodeId = id;
        return copy;
    }

    [[nodiscard]] BasicNodeConfig withWifi(std::string ssid, std::string password) const
    {
        BasicNodeConfig copy = *this;
        copy.wifiSsid = std::move(ssid);
        copy.wifiPassword = std::move(password);
        return copy;
    }

    [[nodiscard]] BasicNodeConfig withBroker(std::string host, const int port) const
    {
        BasicNodeConfig copy = *this;
        copy.brokerHost = std::move(host);
        copy.brokerPort = port;
        return copy;
    }

    [[nodiscard]] BasicNodeConfig withChannelAddress(const int channel, const int address) const
    {
        BasicNodeConfig copy = *this;
        if (channel >= 1 && channel <= kChannelCount)
        {
            copy.channelTurnoutAddresses[channel - 1] = address;
        }
        return copy;
    }

    [[nodiscard]] std::vector<std::string> validate() const
    {
        std::vector<std::string> errors;

        if (nodeId < kMinNodeId || nodeId > kMaxNodeId)
        {
            errors.push_back("node id must be between " + std::to_string(kMinNodeId) +
                              " and " + std::to_string(kMaxNodeId));
        }

        if (wifiSsid.empty())
        {
            errors.push_back("wifi ssid must not be empty");
        }

        if (brokerHost.empty())
        {
            errors.push_back("broker host must not be empty");
        }

        if (brokerPort < kMinBrokerPort || brokerPort > kMaxBrokerPort)
        {
            errors.push_back("broker port must be between " + std::to_string(kMinBrokerPort) +
                              " and " + std::to_string(kMaxBrokerPort));
        }

        for (int i = 0; i < kChannelCount; ++i)
        {
            const int address = channelTurnoutAddresses[i];
            if (address == 0)
            {
                continue;
            }
            if (address < kMinTurnoutAddress || address > kMaxTurnoutAddress)
            {
                errors.push_back("channel " + std::to_string(i + 1) + " address must be between " +
                                  std::to_string(kMinTurnoutAddress) + " and " +
                                  std::to_string(kMaxTurnoutAddress));
            }
            for (int j = i + 1; j < kChannelCount; ++j)
            {
                if (channelTurnoutAddresses[j] == address)
                {
                    errors.push_back("channels " + std::to_string(i + 1) + " and " +
                                      std::to_string(j + 1) + " both claim turnout address " +
                                      std::to_string(address));
                }
            }
        }

        return errors;
    }
};

using NodeConfig = BasicNodeConfig<12>;
using NodeConfig4 = BasicNodeConfig<4>;
