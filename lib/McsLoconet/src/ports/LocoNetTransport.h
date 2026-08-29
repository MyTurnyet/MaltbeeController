#pragma once

#include "domain/Turnout.h"

struct LocoNetPacket
{
    int address;
    TurnoutPosition position;
};

class LocoNetTransport
{
public:
    virtual ~LocoNetTransport() = default;

    virtual void sendPacket(const LocoNetPacket& packet) = 0;
};
