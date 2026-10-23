#pragma once

#include <array>

#include "../ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

template <int N>
class BasicLoco2MqttTurnoutCommandAdapter final : public TurnoutCommandPort
{
public:
    BasicLoco2MqttTurnoutCommandAdapter(MqttTransport& transport, const std::array<int, N>& channelTurnoutAddresses);

    void send(int address, TurnoutPosition position) override;

private:
    MqttTransport& transport_;
    const std::array<int, N>& channelTurnoutAddresses_;
};

using Loco2MqttTurnoutCommandAdapter = BasicLoco2MqttTurnoutCommandAdapter<12>;
using Loco2MqttTurnoutCommandAdapter4 = BasicLoco2MqttTurnoutCommandAdapter<4>;
