#include "MqttPresenceAnnouncer.h"

MqttPresenceAnnouncer::MqttPresenceAnnouncer(MqttTransport& transport, const int nodeId, std::string ownMac)
    : transport_(transport), nodeId_(nodeId), ownMac_(std::move(ownMac))
{
}

void MqttPresenceAnnouncer::update(const bool currentlyConnected)
{
    if (currentlyConnected && !wasConnected_)
    {
        transport_.publish(PresenceTopics::statusTopic(nodeId_), "online", true);
        transport_.publish(PresenceTopics::macTopic(nodeId_), ownMac_, true);
    }
    wasConnected_ = currentlyConnected;
}
