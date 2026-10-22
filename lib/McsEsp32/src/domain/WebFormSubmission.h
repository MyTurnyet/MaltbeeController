#pragma once

#include <array>
#include <string>

#include "NodeConfig.h"

template <int N>
struct BasicWebFormSubmission
{
    std::string nodeId;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string brokerHost;
    std::string brokerPort;
    std::array<std::string, N> channelTurnoutAddresses;
};

using WebFormSubmission = BasicWebFormSubmission<12>;
using WebFormSubmission4 = BasicWebFormSubmission<4>;
