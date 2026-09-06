#pragma once

#include <string>

#include "../domain/PresenceTopics.h"
#include "../ports/MqttTransport.h"
#include "ports/Clock.h"

class MqttPresenceAnnouncer
{
public:
    static constexpr unsigned long kHeartbeatIntervalMs = 30000;

    MqttPresenceAnnouncer(MqttTransport& transport, Clock& clock, int nodeId, std::string ownMac);

    void update(bool currentlyConnected);

private:
    void announce();

    MqttTransport& transport_;
    Clock& clock_;
    int nodeId_;
    std::string ownMac_;
    bool wasConnected_ = false;
    unsigned long lastAnnouncedAtMs_ = 0;
};
