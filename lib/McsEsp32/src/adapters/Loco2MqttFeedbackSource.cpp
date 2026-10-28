#include "Loco2MqttFeedbackSource.h"

namespace
{
    TurnoutPosition toTurnoutPosition(const Loco2MqttPosition position)
    {
        return position == Loco2MqttPosition::Closed ? TurnoutPosition::Closed : TurnoutPosition::Thrown;
    }
}

template <int N>
BasicLoco2MqttFeedbackSource<N>::BasicLoco2MqttFeedbackSource(
    MqttTransport& transport, const std::array<int, N>& channelTurnoutAddresses)
    : stateSource_(transport, channelTurnoutAddresses)
{
}

template <int N>
bool BasicLoco2MqttFeedbackSource<N>::poll(TurnoutFeedback& outFeedback)
{
    Loco2MqttStateUpdate update{};
    if (!stateSource_.poll(update))
    {
        return false;
    }

    outFeedback = TurnoutFeedback{update.channel, toTurnoutPosition(update.position)};
    return true;
}

template class BasicLoco2MqttFeedbackSource<12>;
template class BasicLoco2MqttFeedbackSource<4>;
