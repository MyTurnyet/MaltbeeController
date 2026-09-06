#pragma once

#include <array>
#include <string>

#include "../domain/NodeConfig.h"
#include "../ports/MqttTransport.h"
#include "ports/TurnoutCommandPort.h"

class JmriTurnoutCommandAdapter final : public TurnoutCommandPort
{
public:
    JmriTurnoutCommandAdapter(MqttTransport& transport,
                               const std::array<std::string, NodeConfig::kChannelCount>& channelJmriNames);

    void send(int address, TurnoutPosition position) override;

private:
    MqttTransport& transport_;
    const std::array<std::string, NodeConfig::kChannelCount>& channelJmriNames_;
};
