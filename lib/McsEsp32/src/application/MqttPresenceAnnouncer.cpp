#include "MqttPresenceAnnouncer.h"

MqttPresenceAnnouncer::MqttPresenceAnnouncer(MqttTransport& transport, Clock& clock, const int nodeId,
                                              std::string ownMac)
    : transport_(transport), clock_(clock), nodeId_(nodeId), ownMac_(std::move(ownMac))
{
}

void MqttPresenceAnnouncer::update(const bool currentlyConnected)
{
    if (!currentlyConnected)
    {
        wasConnected_ = false;
        return;
    }

    const bool justConnected = !wasConnected_;
    const bool heartbeatDue = clock_.nowMilliseconds() - lastAnnouncedAtMs_ >= kHeartbeatIntervalMs;

    if (justConnected || heartbeatDue)
    {
        announce();
    }

    wasConnected_ = true;
}

void MqttPresenceAnnouncer::announce()
{
    transport_.publish(PresenceTopics::statusTopic(nodeId_), "online", false);
    transport_.publish(PresenceTopics::macTopic(nodeId_), ownMac_, false);
    lastAnnouncedAtMs_ = clock_.nowMilliseconds();
}
