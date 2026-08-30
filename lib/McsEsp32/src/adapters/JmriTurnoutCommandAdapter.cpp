#include "JmriTurnoutCommandAdapter.h"

#include "../domain/PayloadCodec.h"
#include "../domain/TopicScheme.h"

JmriTurnoutCommandAdapter::JmriTurnoutCommandAdapter(
    MqttTransport& transport, const std::array<std::string, NodeConfig::kChannelCount>& channelJmriNames)
    : transport_(transport), channelJmriNames_(channelJmriNames)
{
}

void JmriTurnoutCommandAdapter::send(const int address, const TurnoutPosition position)
{
    if (address < 1 || address > NodeConfig::kChannelCount)
    {
        return;
    }

    const std::string& jmriName = channelJmriNames_[address - 1];
    if (jmriName.empty())
    {
        return;
    }

    transport_.publish(TopicScheme::topicFor(jmriName), PayloadCodec::encode(position), false);
}
