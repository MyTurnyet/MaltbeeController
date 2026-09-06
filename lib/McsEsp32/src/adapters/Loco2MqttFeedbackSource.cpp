#include "Loco2MqttFeedbackSource.h"

#include <optional>

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

Loco2MqttFeedbackSource::Loco2MqttFeedbackSource(
    MqttTransport& transport, const std::array<int, NodeConfig::kChannelCount>& channelTurnoutAddresses)
{
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
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

bool Loco2MqttFeedbackSource::poll(TurnoutFeedback& outFeedback)
{
    if (pending_.empty())
    {
        return false;
    }

    outFeedback = pending_.front();
    pending_.pop_front();
    return true;
}
