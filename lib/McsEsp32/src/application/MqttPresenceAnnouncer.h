#pragma once

#include <string>

#include "../domain/PresenceTopics.h"
#include "../ports/MqttTransport.h"

class MqttPresenceAnnouncer
{
public:
    MqttPresenceAnnouncer(MqttTransport& transport, int nodeId, std::string ownMac);

    void update(bool currentlyConnected);

private:
    MqttTransport& transport_;
    int nodeId_;
    std::string ownMac_;
    bool wasConnected_ = false;
};
