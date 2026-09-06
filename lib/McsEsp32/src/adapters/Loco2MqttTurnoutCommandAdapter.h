#pragma once

#include <array>

#include "../domain/NodeConfig.h"
#include "../ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class Loco2MqttTurnoutCommandAdapter final : public TurnoutCommandPort
{
public:
    Loco2MqttTurnoutCommandAdapter(MqttTransport& transport,
                                    const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses);

    void send(int address, TurnoutPosition position) override;

private:
    MqttTransport& transport_;
    const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses_;
};
