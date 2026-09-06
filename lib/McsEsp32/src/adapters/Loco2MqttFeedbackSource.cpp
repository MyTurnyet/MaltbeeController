#include "JmriFeedbackSource.h"

#include <optional>

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

JmriFeedbackSource::JmriFeedbackSource(MqttTransport& transport,
                                        const std::array<std::string, NodeConfig::kChannelCount>& channelJmriNames)
{
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string& jmriName = channelJmriNames[i];
        if (jmriName.empty())
        {
            continue;
        }

        const int channel = i + 1;
        transport.subscribe(TopicScheme::stateTopicFor(jmriName), [this, channel](const std::string& payload) {
            const std::optional<TurnoutPosition> position = PayloadCodec::decode(payload);
            if (!position.has_value())
            {
                return;
            }
            pending_.push_back(TurnoutFeedback{channel, *position});
        });
    }
}

bool JmriFeedbackSource::poll(TurnoutFeedback& outFeedback)
{
    if (pending_.empty())
    {
        return false;
    }

    outFeedback = pending_.front();
    pending_.pop_front();
    return true;
}
