#include "Loco2MqttTurnoutCommandAdapter.h"

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

template <int N>
BasicLoco2MqttTurnoutCommandAdapter<N>::BasicLoco2MqttTurnoutCommandAdapter(
    MqttTransport& transport, const std::array<int, N>& channelTurnoutAddresses)
    : transport_(transport), channelTurnoutAddresses_(channelTurnoutAddresses)
{
}

template <int N>
void BasicLoco2MqttTurnoutCommandAdapter<N>::send(const int address, const TurnoutPosition position)
{
    if (address < 1 || address > N)
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

template class BasicLoco2MqttTurnoutCommandAdapter<12>;
template class BasicLoco2MqttTurnoutCommandAdapter<4>;
