#pragma once

#include <array>
#include <deque>

#include "../domain/NodeConfig.h"
#include "../ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class Loco2MqttFeedbackSource
{
public:
    Loco2MqttFeedbackSource(MqttTransport& transport,
                             const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses);

    bool poll(TurnoutFeedback& outFeedback);

private:
    std::deque<TurnoutFeedback> pending_;
};
