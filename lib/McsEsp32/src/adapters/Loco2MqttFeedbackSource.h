#pragma once

#include <array>

#include "adapters/Loco2MqttStateSource.h"
#include "ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

// Translates the shared mcs-net state source into this project's TurnoutFeedback.
template <int N>
class BasicLoco2MqttFeedbackSource
{
public:
    BasicLoco2MqttFeedbackSource(MqttTransport& transport, const std::array<int, N>& channelTurnoutAddresses);

    bool poll(TurnoutFeedback& outFeedback);

private:
    BasicLoco2MqttStateSource<N> stateSource_;
};

using Loco2MqttFeedbackSource = BasicLoco2MqttFeedbackSource<12>;
using Loco2MqttFeedbackSource4 = BasicLoco2MqttFeedbackSource<4>;
