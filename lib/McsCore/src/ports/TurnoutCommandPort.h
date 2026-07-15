#pragma once

#include "../domain/Turnout.h"

class TurnoutCommandPort
{
public:
    virtual ~TurnoutCommandPort() = default;

    virtual void send(int address, TurnoutPosition position) = 0;
};

struct TurnoutFeedback
{
    int address;
    TurnoutPosition position;
};
