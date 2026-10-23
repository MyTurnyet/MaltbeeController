#pragma once

#include <array>
#include <deque>

#include "../ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

template <int N>
class BasicLoco2MqttFeedbackSource
{
public:
    BasicLoco2MqttFeedbackSource(MqttTransport& transport, const std::array<int, N>& channelTurnoutAddresses);

    bool poll(TurnoutFeedback& outFeedback);

private:
    std::deque<TurnoutFeedback> pending_;
};

using Loco2MqttFeedbackSource = BasicLoco2MqttFeedbackSource<12>;
using Loco2MqttFeedbackSource4 = BasicLoco2MqttFeedbackSource<4>;
