#include "NodeConfig.h"

NodeConfig NodeConfig::factoryDefault()
{
    return NodeConfig{};
}

NodeConfig NodeConfig::withNodeId(const int id) const
{
    NodeConfig copy = *this;
    copy.nodeId = id;
    return copy;
}

NodeConfig NodeConfig::withWifi(std::string ssid, std::string password) const
{
    NodeConfig copy = *this;
    copy.wifiSsid = std::move(ssid);
    copy.wifiPassword = std::move(password);
    return copy;
}

NodeConfig NodeConfig::withBroker(std::string host, const int port) const
{
    NodeConfig copy = *this;
    copy.brokerHost = std::move(host);
    copy.brokerPort = port;
    return copy;
}

NodeConfig NodeConfig::withChannelName(const int channel, std::string jmriName) const
{
    NodeConfig copy = *this;
    if (channel >= 1 && channel <= kChannelCount)
    {
        copy.channelJmriNames[channel - 1] = std::move(jmriName);
    }
    return copy;
}

std::vector<std::string> NodeConfig::validate() const
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
        if (channelJmriNames[i].empty())
        {
            continue;
        }
        for (int j = i + 1; j < kChannelCount; ++j)
        {
            if (channelJmriNames[i] == channelJmriNames[j])
            {
                errors.push_back("channels " + std::to_string(i + 1) + " and " +
                                  std::to_string(j + 1) + " both claim jmri name \"" +
                                  channelJmriNames[i] + "\"");
            }
        }
    }

    return errors;
}