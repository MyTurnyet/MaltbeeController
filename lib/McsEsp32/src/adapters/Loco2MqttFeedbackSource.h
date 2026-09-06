#pragma once

#include <array>
#include <deque>
#include <string>

#include "../domain/NodeConfig.h"
#include "../ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class JmriFeedbackSource
{
public:
    JmriFeedbackSource(MqttTransport& transport,
                        const std::array<std::string, NodeConfig::kChannelCount>& channelJmriNames);

    bool poll(TurnoutFeedback& outFeedback);

private:
    std::deque<TurnoutFeedback> pending_;
};
