#pragma once

#include <vector>

#include "ports/TurnoutCommandPort.h"

class FakeTurnoutCommandPort final : public TurnoutCommandPort
{
public:
    std::vector<TurnoutFeedback> sentCommands;

    void send(const int address, const TurnoutPosition position) override
    {
        sentCommands.push_back({address, position});
    }
};
