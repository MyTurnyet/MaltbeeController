#include "Loco2MqttFeedbackSource.h"

#include <optional>

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

template <int N>
BasicLoco2MqttFeedbackSource<N>::BasicLoco2MqttFeedbackSource(
    MqttTransport& transport, const std::array<int, N>& channelTurnoutAddresses)
{
    for (int i = 0; i < N; ++i)
    {
        const int turnoutAddress = channelTurnoutAddresses[i];
        if (turnoutAddress == 0)
        {
            continue;
        }

        const int channel = i + 1;
        transport.subscribe(TopicScheme::stateTopicFor(turnoutAddress), [this, channel](const std::string& payload) {
            const std::optional<TurnoutPosition> position = PayloadCodec::decode(payload);
            if (!position.has_value())
            {
                return;
            }
            pending_.push_back(TurnoutFeedback{channel, *position});
        });
    }
}

template <int N>
bool BasicLoco2MqttFeedbackSource<N>::poll(TurnoutFeedback& outFeedback)
{
    if (pending_.empty())
    {
        return false;
    }

    outFeedback = pending_.front();
    pending_.pop_front();
    return true;
}

template class BasicLoco2MqttFeedbackSource<12>;
template class BasicLoco2MqttFeedbackSource<4>;
