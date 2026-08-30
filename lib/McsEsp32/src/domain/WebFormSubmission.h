#pragma once

#include <array>
#include <string>

#include "NodeConfig.h"

struct WebFormSubmission
{
    std::string nodeId;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    std::string brokerPort;
    std::array<std::string, NodeConfig::kChannelCount> channelJmriNames;
};
