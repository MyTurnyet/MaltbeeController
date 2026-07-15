#pragma once

#include "../ports/TurnoutCommandPort.h"

class NullTurnoutCommandPort final : public TurnoutCommandPort
{
public:
    void send(int address, TurnoutPosition position) override
    {
    }
};
