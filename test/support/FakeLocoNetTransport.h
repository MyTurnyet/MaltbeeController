#pragma once

#include <vector>

#include "ports/LocoNetTransport.h"

class FakeLocoNetTransport final : public LocoNetTransport
{
public:
    std::vector<LocoNetPacket> sentPackets;

    void sendPacket(const LocoNetPacket& packet) override
    {
        sentPackets.push_back(packet);
    }
};
