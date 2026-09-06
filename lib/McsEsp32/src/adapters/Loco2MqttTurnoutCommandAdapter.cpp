#include "Loco2MqttTurnoutCommandAdapter.h"

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

Loco2MqttTurnoutCommandAdapter::Loco2MqttTurnoutCommandAdapter(
    MqttTransport& transport, const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses)
    : transport_(transport), channelTurnoutAddresses_(channelTurnoutAddresses)
{
}

void Loco2MqttTurnoutCommandAdapter::send(const int address, const TurnoutPosition position)
{
    if (address < 1 || address > NodeConfig::kChannelCount)
    {
        return;
    }

    const int turnoutAddress = channelTurnoutAddresses_[address - 1];
    if (turnoutAddress == 0)
    {
        return;
    }

    transport_.publish(TopicScheme::topicFor(turnoutAddress), PayloadCodec::encode(position), false);
}
